// SPDX-License-Identifier: MIT
//! OS glue: tray icon, single-instance guard, opening URLs, Linux desktop integration.

use std::fs::TryLockError;
use std::io::{BufRead, BufReader, Write};
use std::net::{TcpListener, TcpStream};
use std::path::PathBuf;
use std::sync::mpsc::{Receiver, Sender, channel};
use std::sync::{Mutex, OnceLock};
use std::time::Duration;

pub const ICON_PNG: &[u8] = include_bytes!("../resources/icons/app.png");

/// Requests from outside the UI thread: tray menu and later launches.
#[derive(Clone, Copy, Debug)]
pub enum External {
    ShowMain,
    CheckUpdates,
    Quit,
}

type Channel = (Mutex<Sender<External>>, Mutex<Receiver<External>>);
static EXTERNAL: OnceLock<Channel> = OnceLock::new();

fn external() -> &'static Channel {
    EXTERNAL.get_or_init(|| {
        let (tx, rx) = channel();
        (Mutex::new(tx), Mutex::new(rx))
    })
}

pub fn send(e: External) {
    if let Ok(tx) = external().0.lock() {
        let _ = tx.send(e);
    }
}

pub fn drain() -> Vec<External> {
    external()
        .1
        .lock()
        .map(|rx| rx.try_iter().collect())
        .unwrap_or_default()
}

pub fn icon_rgba() -> (Vec<u8>, u32, u32) {
    let img = image::load_from_memory(ICON_PNG)
        .expect("embedded icon")
        .to_rgba8();
    let (w, h) = img.dimensions();
    (img.into_raw(), w, h)
}

pub fn window_icon() -> Option<iced::window::Icon> {
    let (rgba, w, h) = icon_rgba();
    iced::window::icon::from_rgba(rgba, w, h).ok()
}

pub fn open_url(url: &str) {
    #[cfg(windows)]
    let _ = std::process::Command::new("explorer").arg(url).spawn();
    #[cfg(not(windows))]
    let _ = std::process::Command::new("xdg-open").arg(url).spawn();
}

// ---- single instance ----

/// Held for the life of the process; the OS drops the lock if it dies, so it never goes stale.
pub struct InstanceGuard {
    _lock: Option<std::fs::File>,
}

fn instance_paths() -> (PathBuf, PathBuf) {
    let user = std::env::var("USER")
        .or_else(|_| std::env::var("USERNAME"))
        .unwrap_or_default();
    // One name per user, so two accounts don't talk to each other's copy.
    let tag: String = user
        .chars()
        .filter(|c| c.is_ascii_alphanumeric())
        .take(32)
        .collect();
    let dir = dirs::runtime_dir().unwrap_or_else(std::env::temp_dir);
    (
        dir.join(format!("remindme-{tag}.lock")),
        dir.join(format!("remindme-{tag}.port")),
    )
}

/// A visible error: the Windows GUI build has no console, so stderr goes nowhere.
pub fn show_error(text: &str) {
    #[cfg(windows)]
    {
        #[link(name = "user32")]
        unsafe extern "system" {
            fn MessageBoxW(
                hwnd: *mut std::ffi::c_void,
                text: *const u16,
                caption: *const u16,
                flags: u32,
            ) -> i32;
        }
        let wide = |s: &str| s.encode_utf16().chain([0]).collect::<Vec<u16>>();
        let (text, caption) = (wide(text), wide(remindme::APP_NAME));
        // 0x10 = MB_ICONERROR
        unsafe { MessageBoxW(std::ptr::null_mut(), text.as_ptr(), caption.as_ptr(), 0x10) };
    }
    #[cfg(not(windows))]
    eprintln!("{text}");
}

pub enum Instance {
    First(InstanceGuard),
    /// Another copy is running; it was asked to show itself (true when it answered).
    Other(bool),
}

pub fn claim_instance() -> Instance {
    let (lock_path, port_path) = instance_paths();
    let Ok(lock) = std::fs::OpenOptions::new()
        .create(true)
        .truncate(false)
        .write(true)
        .open(&lock_path)
    else {
        // No writable lock location: run unguarded rather than not at all.
        return Instance::First(InstanceGuard { _lock: None });
    };
    // Right after a self-update the old copy is still quitting: wait for its lock instead of handing off to it.
    let mut state = lock.try_lock();
    if matches!(state, Err(TryLockError::WouldBlock))
        && std::env::var_os("REMINDME_AFTER_UPDATE").is_some()
    {
        for _ in 0..50 {
            std::thread::sleep(Duration::from_millis(200));
            state = lock.try_lock();
            if !matches!(state, Err(TryLockError::WouldBlock)) {
                break;
            }
        }
    }
    match state {
        Ok(()) => {}
        // Only contention means another copy runs; any other error is like not being able to open the file.
        Err(TryLockError::WouldBlock) => {
            return Instance::Other(ask_running_copy_to_show(&port_path));
        }
        Err(TryLockError::Error(_)) => return Instance::First(InstanceGuard { _lock: None }),
    }
    // Later launches connect here and ask this copy to show itself instead of starting another.
    if let Ok(listener) = TcpListener::bind(("127.0.0.1", 0))
        && let Ok(addr) = listener.local_addr()
        && std::fs::write(&port_path, addr.port().to_string()).is_ok()
    {
        std::thread::spawn(move || {
            for stream in listener.incoming().flatten() {
                let _ = stream.set_read_timeout(Some(Duration::from_secs(2)));
                let mut line = String::new();
                let mut reader = BufReader::new(&stream);
                if reader.read_line(&mut line).is_ok() && line.starts_with("show") {
                    send(External::ShowMain);
                    let _ = (&stream).write_all(b"ok\n");
                }
            }
        });
    }
    Instance::First(InstanceGuard { _lock: Some(lock) })
}

fn ask_running_copy_to_show(port_path: &PathBuf) -> bool {
    let Some(port) = std::fs::read_to_string(port_path)
        .ok()
        .and_then(|p| p.trim().parse::<u16>().ok())
    else {
        return false;
    };
    let Ok(mut stream) = TcpStream::connect_timeout(
        &(std::net::Ipv4Addr::LOCALHOST, port).into(),
        Duration::from_secs(2),
    ) else {
        return false;
    };
    let _ = stream.set_read_timeout(Some(Duration::from_secs(2)));
    let mut reply = String::new();
    stream.write_all(b"show\n").is_ok()
        && BufReader::new(stream).read_line(&mut reply).is_ok()
        && reply.starts_with("ok")
}

// ---- tray ----

pub struct Tray {
    #[cfg(windows)]
    _icon: tray_icon::TrayIcon,
    #[cfg(target_os = "linux")]
    _handle: ksni::blocking::Handle<LinuxTray>,
}

#[cfg(target_os = "linux")]
pub struct LinuxTray {
    icon: ksni::Icon,
}

#[cfg(target_os = "linux")]
impl ksni::Tray for LinuxTray {
    fn id(&self) -> String {
        "remindme".into()
    }
    fn title(&self) -> String {
        remindme::APP_NAME.into()
    }
    fn icon_pixmap(&self) -> Vec<ksni::Icon> {
        vec![self.icon.clone()]
    }
    fn tool_tip(&self) -> ksni::ToolTip {
        ksni::ToolTip {
            title: remindme::APP_NAME.into(),
            ..Default::default()
        }
    }
    fn activate(&mut self, _x: i32, _y: i32) {
        send(External::ShowMain);
    }
    fn menu(&self) -> Vec<ksni::MenuItem<Self>> {
        use ksni::menu::StandardItem;
        let item = |label: &str, e: External| {
            StandardItem {
                label: label.into(),
                activate: Box::new(move |_: &mut Self| send(e)),
                ..Default::default()
            }
            .into()
        };
        vec![
            item("Open RemindMe", External::ShowMain),
            item("Check for Updates", External::CheckUpdates),
            ksni::MenuItem::Separator,
            item("Quit", External::Quit),
        ]
    }
}

/// None when the desktop has no tray (e.g. GNOME without the AppIndicator extension).
#[cfg(target_os = "linux")]
pub fn create_tray() -> Option<Tray> {
    use ksni::blocking::TrayMethods;
    let (rgba, w, h) = icon_rgba();
    // SNI wants ARGB32 in network byte order.
    let data = rgba
        .as_chunks::<4>()
        .0
        .iter()
        .flat_map(|p| [p[3], p[0], p[1], p[2]])
        .collect();
    let tray = LinuxTray {
        icon: ksni::Icon {
            width: w as i32,
            height: h as i32,
            data,
        },
    };
    tray.spawn().ok().map(|handle| Tray { _handle: handle })
}

#[cfg(windows)]
pub fn create_tray() -> Option<Tray> {
    use tray_icon::menu::{Menu, MenuEvent, MenuItem, PredefinedMenuItem};
    use tray_icon::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent};

    let open = MenuItem::new("Open RemindMe", true, None);
    let updates = MenuItem::new("Check for Updates", true, None);
    let quit = MenuItem::new("Quit", true, None);
    let menu = Menu::new();
    menu.append_items(&[&open, &updates, &PredefinedMenuItem::separator(), &quit])
        .ok()?;
    let ids = [
        (open.id().clone(), External::ShowMain),
        (updates.id().clone(), External::CheckUpdates),
        (quit.id().clone(), External::Quit),
    ];
    MenuEvent::set_event_handler(Some(move |e: MenuEvent| {
        if let Some((_, ext)) = ids.iter().find(|(id, _)| *id == e.id) {
            send(*ext);
        }
    }));
    TrayIconEvent::set_event_handler(Some(|e: TrayIconEvent| {
        if let TrayIconEvent::Click {
            button: MouseButton::Left,
            button_state: MouseButtonState::Up,
            ..
        }
        | TrayIconEvent::DoubleClick {
            button: MouseButton::Left,
            ..
        } = e
        {
            send(External::ShowMain);
        }
    }));
    let (rgba, w, h) = icon_rgba();
    let icon = tray_icon::Icon::from_rgba(rgba, w, h).ok()?;
    let tray = TrayIconBuilder::new()
        .with_menu(Box::new(menu))
        .with_menu_on_left_click(false)
        .with_tooltip(remindme::APP_NAME)
        .with_icon(icon)
        .build()
        .ok()?;
    Some(Tray { _icon: tray })
}

// ---- Linux AppImage desktop integration ----

/// Running as an AppImage: keep a launcher entry and icon pointing at this file, so it shows in the app menu.
#[cfg(target_os = "linux")]
pub fn integrate_appimage() {
    let Ok(appimage) = std::env::var("APPIMAGE") else {
        return;
    };
    let Some(data) = dirs::data_dir() else { return };
    let icon_dir = data.join("icons/hicolor/256x256/apps");
    let apps_dir = data.join("applications");
    // Absolute icon path: a stale user icon-theme.cache hides icons looked up by name.
    // Exec quoting (\ " ` $ escaped with a backslash, % doubled), then the desktop-file string rule doubles each backslash.
    let mut exec = String::new();
    for c in appimage.chars() {
        match c {
            '\\' => exec.push_str("\\\\\\\\"),
            '"' | '`' | '$' => {
                exec.push_str("\\\\");
                exec.push(c);
            }
            '%' => exec.push_str("%%"),
            _ => exec.push(c),
        }
    }
    let icon = icon_dir.join("remindme.png");
    let entry = format!(
        "[Desktop Entry]\nType=Application\nName=RemindMe\nComment=Natural-language reminders\nExec=\"{exec}\"\nIcon={}\nTerminal=false\nCategories=Utility;\nStartupWMClass=remindme\n",
        icon.display()
    );
    if std::fs::read(&icon).ok().as_deref() != Some(ICON_PNG) {
        let _ = std::fs::create_dir_all(&icon_dir).and_then(|_| std::fs::write(&icon, ICON_PNG));
    }
    let path = apps_dir.join("remindme.desktop");
    if std::fs::read_to_string(&path).ok().as_deref() != Some(entry.as_str()) {
        let _ = std::fs::create_dir_all(&apps_dir).and_then(|_| std::fs::write(path, entry));
    }
}
