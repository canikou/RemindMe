// SPDX-License-Identifier: MIT
//! Window-manager hints for the due popup and the overlay on Linux (X11 / XWayland).
//!
//! Tiling window managers (Hyprland, i3, sway via XWayland) tile every plain window, so the popup
//! filled a whole tile and the overlay took a slot. The windows open hidden, get EWMH hints that
//! every manager reads at map time (dialog / utility type, above, sticky, skip taskbar), and are
//! shown after. Under Hyprland they are also floated, pinned and (the popup) focused through its
//! IPC socket, because Hyprland does not focus or raise windows that ask for it by default. Both
//! dispatch syntaxes are handled: Lua configs (0.55+) and classic hyprland.conf.

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Role {
    Popup,
    Overlay,
}

/// Sets the window type and initial state on the unmapped X11 window `xid`.
/// Does nothing (and logs under REMINDME_DEBUG) when there is no X server to talk to.
#[cfg(target_os = "linux")]
pub fn prepare(xid: u64, role: Role) {
    if let Err(e) = x11::prepare(xid, role) {
        debug(&format!("x11 hints for {role:?} failed: {e}"));
    }
}

/// Hyprland only: floats and pins the window once it is mapped, and focuses the popup.
/// `matches` picks this window out of our own windows by title. Runs on a detached thread.
#[cfg(target_os = "linux")]
pub fn after_map(role: Role, matches: fn(&str) -> bool) {
    let Some(socket) = hyprland::socket() else {
        return;
    };
    std::thread::spawn(move || {
        if let Err(e) = hyprland::settle(&socket, role, matches) {
            debug(&format!("hyprland setup for {role:?} failed: {e}"));
        }
    });
}

#[cfg(target_os = "linux")]
fn debug(msg: &str) {
    if std::env::var_os("REMINDME_DEBUG").is_some() {
        eprintln!("[debug] {msg}");
    }
}

#[cfg(target_os = "linux")]
mod x11 {
    use super::Role;
    use x11rb::connection::Connection;
    use x11rb::protocol::xproto::{AtomEnum, ConnectionExt, PropMode};
    use x11rb::wrapper::ConnectionExt as _;

    pub fn prepare(xid: u64, role: Role) -> Result<(), Box<dyn std::error::Error>> {
        let (conn, _) = x11rb::connect(None)?;
        let window = u32::try_from(xid)?;
        let atom = |name: &str| -> Result<u32, Box<dyn std::error::Error>> {
            Ok(conn.intern_atom(false, name.as_bytes())?.reply()?.atom)
        };
        let (kind, states): (&str, &[&str]) = match role {
            Role::Popup => (
                "_NET_WM_WINDOW_TYPE_DIALOG",
                &["_NET_WM_STATE_ABOVE", "_NET_WM_STATE_STICKY"],
            ),
            Role::Overlay => (
                "_NET_WM_WINDOW_TYPE_UTILITY",
                &[
                    "_NET_WM_STATE_ABOVE",
                    "_NET_WM_STATE_STICKY",
                    "_NET_WM_STATE_SKIP_TASKBAR",
                    "_NET_WM_STATE_SKIP_PAGER",
                ],
            ),
        };
        let kind = atom(kind)?;
        let states = states
            .iter()
            .map(|s| atom(s))
            .collect::<Result<Vec<_>, _>>()?;
        conn.change_property32(
            PropMode::REPLACE,
            window,
            atom("_NET_WM_WINDOW_TYPE")?,
            AtomEnum::ATOM,
            &[kind],
        )?;
        conn.change_property32(
            PropMode::REPLACE,
            window,
            atom("_NET_WM_STATE")?,
            AtomEnum::ATOM,
            &states,
        )?;
        conn.flush()?;
        // Make sure the server has applied them before the window gets mapped.
        conn.get_input_focus()?.reply()?;
        Ok(())
    }
}

#[cfg(target_os = "linux")]
mod hyprland {
    use super::Role;
    use std::io::{Read, Write};
    use std::os::unix::net::UnixStream;
    use std::path::PathBuf;
    use std::sync::atomic::{AtomicU8, Ordering};
    use std::time::Duration;

    type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

    /// The control socket of the Hyprland session we run in, if any.
    pub fn socket() -> Option<PathBuf> {
        let sig = std::env::var_os("HYPRLAND_INSTANCE_SIGNATURE")?;
        let mut dirs = Vec::new();
        if let Some(run) = std::env::var_os("XDG_RUNTIME_DIR") {
            dirs.push(PathBuf::from(run).join("hypr"));
        }
        dirs.push(PathBuf::from("/tmp/hypr")); // Hyprland before 0.40
        dirs.into_iter()
            .map(|d| d.join(&sig).join(".socket.sock"))
            .find(|p| p.exists())
    }

    fn request(socket: &PathBuf, cmd: &str) -> Result<String> {
        let mut s = UnixStream::connect(socket)?;
        s.set_read_timeout(Some(Duration::from_secs(2)))?;
        s.write_all(cmd.as_bytes())?;
        let mut out = String::new();
        s.read_to_string(&mut out)?;
        Ok(out)
    }

    /// Which dispatch syntax this Hyprland speaks: 0 unknown, 1 Lua (0.55+ with a Lua config),
    /// 2 classic hyprland.conf. Learned from the first command that succeeds.
    static SYNTAX: AtomicU8 = AtomicU8::new(0);
    const LUA: u8 = 1;
    const CLASSIC: u8 = 2;

    fn send(socket: &PathBuf, args: &str) -> Result<()> {
        let reply = request(socket, &format!("dispatch {args}"))?;
        if reply.trim() != "ok" {
            return Err(format!("`dispatch {args}`: {}", reply.trim()).into());
        }
        Ok(())
    }

    /// Runs one action in whichever syntax this Hyprland accepts. Lua configs turn
    /// `dispatch X` into `hl.dispatch(X)`, so the classic form fails there and vice versa.
    fn dispatch(socket: &PathBuf, lua: &str, classic: &str) -> Result<()> {
        let order = match SYNTAX.load(Ordering::Relaxed) {
            CLASSIC => [(CLASSIC, classic), (LUA, lua)],
            _ => [(LUA, lua), (CLASSIC, classic)],
        };
        let mut errors = Vec::new();
        for (syntax, args) in order {
            match send(socket, args) {
                Ok(()) => {
                    SYNTAX.store(syntax, Ordering::Relaxed);
                    return Ok(());
                }
                Err(e) => errors.push(e.to_string()),
            }
        }
        Err(errors.join("; ").into())
    }

    fn lua_call(function: &str, target: &str, extra: &str) -> String {
        format!("{function}({{ window = \"{target}\"{extra} }})")
    }

    /// One of our windows as Hyprland lists it.
    #[derive(Debug, PartialEq)]
    pub struct Client {
        pub address: String,
        pub floating: bool,
        pub pinned: bool,
    }

    /// Finds the window of process `pid` whose title `matches` in a `j/clients` reply.
    pub fn find(clients: &str, pid: u32, matches: fn(&str) -> bool) -> Option<Client> {
        let list: serde_json::Value = serde_json::from_str(clients).ok()?;
        list.as_array()?.iter().find_map(|c| {
            let ours = c["pid"].as_u64() == Some(u64::from(pid))
                && c["mapped"].as_bool() != Some(false)
                && matches(c["title"].as_str().unwrap_or_default());
            ours.then(|| Client {
                address: c["address"].as_str().unwrap_or_default().to_string(),
                floating: c["floating"].as_bool().unwrap_or(false),
                pinned: c["pinned"].as_bool().unwrap_or(false),
            })
        })
    }

    pub fn settle(socket: &PathBuf, role: Role, matches: fn(&str) -> bool) -> Result<()> {
        let pid = std::process::id();
        // The window shows up in the client list shortly after it is mapped.
        let mut client = None;
        for _ in 0..40 {
            client = find(&request(socket, "j/clients")?, pid, matches);
            if client.is_some() {
                break;
            }
            std::thread::sleep(Duration::from_millis(50));
        }
        let Some(c) = client else {
            return Err("window not found in the client list".into());
        };
        let target = format!("address:{}", c.address);
        // Each step is tried on its own: a failed pin must not keep the popup from getting focus.
        let mut steps: Vec<(String, String)> = Vec::new();
        if !c.floating {
            steps.push((
                lua_call("hl.dsp.window.float", &target, ", action = \"enable\""),
                format!("setfloating {target}"),
            ));
        }
        // Classic `pin` only toggles, so only pin a window that is not pinned yet.
        if !c.pinned {
            steps.push((
                lua_call("hl.dsp.window.pin", &target, ", action = \"enable\""),
                format!("pin {target}"),
            ));
        }
        if role == Role::Popup {
            steps.push((
                lua_call("hl.dsp.focus", &target, ""),
                format!("focuswindow {target}"),
            ));
            steps.push((
                lua_call("hl.dsp.window.alter_zorder", &target, ", mode = \"top\""),
                format!("alterzorder top,{target}"),
            ));
        }
        let errors: Vec<String> = steps
            .iter()
            .filter_map(|(lua, classic)| dispatch(socket, lua, classic).err())
            .map(|e| e.to_string())
            .collect();
        if errors.is_empty() {
            Ok(())
        } else {
            Err(errors.join(" | ").into())
        }
    }

    #[cfg(test)]
    mod tests {
        use super::*;

        const CLIENTS: &str = r#"[
            {"address":"0x1","mapped":true,"pid":7,"title":"RemindMe","floating":false,"pinned":false},
            {"address":"0x2","mapped":true,"pid":9,"title":"Tea | Today 3:00 PM","floating":false,"pinned":false},
            {"address":"0x3","mapped":true,"pid":7,"title":"Tea | Today 3:00 PM","floating":true,"pinned":false}
        ]"#;

        fn popup_title(t: &str) -> bool {
            t.contains(" | ")
        }

        #[test]
        fn finds_only_our_own_window() {
            let c = find(CLIENTS, 7, popup_title).unwrap();
            assert_eq!(
                c,
                Client {
                    address: "0x3".into(),
                    floating: true,
                    pinned: false
                }
            );
            assert!(find(CLIENTS, 8, popup_title).is_none());
            assert!(find("not json", 7, popup_title).is_none());
        }

        #[test]
        fn lua_calls_target_the_window() {
            assert_eq!(
                lua_call(
                    "hl.dsp.window.float",
                    "address:0x3",
                    ", action = \"enable\""
                ),
                r#"hl.dsp.window.float({ window = "address:0x3", action = "enable" })"#
            );
            assert_eq!(
                lua_call("hl.dsp.focus", "address:0x3", ""),
                r#"hl.dsp.focus({ window = "address:0x3" })"#
            );
        }
    }
}
