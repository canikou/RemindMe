// SPDX-License-Identifier: MIT
#![cfg_attr(windows, windows_subsystem = "windows")]

mod app;
mod flap;
mod platform;
mod theme;
mod updater;

use platform::Instance;

fn main() -> iced::Result {
    let _guard = match platform::claim_instance() {
        Instance::First(guard) => guard,
        // The running copy was asked to show its window; nothing else to do.
        Instance::Other(answered) => {
            if !answered {
                platform::show_error(
                    "RemindMe is already running but didn't respond. Close it and try again.",
                );
            }
            return Ok(());
        }
    };
    #[cfg(target_os = "linux")]
    platform::integrate_appimage();

    // Kept alive for the whole run; None when the desktop has no tray (closing the window then quits).
    let tray = platform::create_tray();
    let has_tray = tray.is_some();

    iced::daemon(
        move || app::App::boot(has_tray),
        app::App::update,
        app::App::view,
    )
    .title(app::App::title)
    .subscription(app::App::subscription)
    .theme(app::App::theme)
    .style(app::App::style)
    .run()
}
