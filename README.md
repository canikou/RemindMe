# RemindMe

RemindMe is a desktop reminders app for Windows and Linux, written in Rust with [iced](https://iced.rs).
Current version: `v2.0.2`.

## User Experience

- Multiple reminders sorted by soonest due time.
- Natural-language parsing for relative reminders (`in ...`) and time-of-day reminders (`at ...`).
- Repeat scheduling via interval syntax (`every 2 hours`) and weekday syntax (`every weekdays`, `every Mon-Fri`, `every Monday through Friday`).
- Integer math support in duration input (for example `(3*10) minutes`).
- Repeating reminder subtasks with progress tracking and per-cycle reset.
- Popup notifications with `Snooze 5 min` and `Done` (Esc snoozes).
- Closing a reminder popup with `X` behaves the same as pressing `OK`.
- Single-instance guard prevents duplicate app processes.
- System tray runtime behavior keeps timers active when the main window is closed.
- Optional always-on-top compact overlay with drag support and guarded title/timer layout to avoid clipping.
- Completed reminder history with quick re-add and a collapsible slide in/slide out preview in the main window.
- Import/export share-string flow for moving reminders between instances.
- Startup update checks against GitHub Releases with user-consent download and installer launch flow, using `setup.exe` on Windows and the AppImage on Linux.
- Persistent JSON storage in Documents with automatic one-time migration from legacy app-data location.

## Input Examples

- `Drink water in 45m`
- `Stretch in 2h 30m`
- `Hydrate in (3*10) minutes`
- `Stand up at 7:00AM`
- `Reinforce on October 24, 2026 6:30AM` (also `on 2026-10-24 18:30`, `at 6:30PM on 5 Dec 2026`)
- `Vacuum room on Sunday 8 AM` (also `Call mom Friday at 7pm`, `Pay rent at 9am next Mon`, `Water plants tomorrow`)
- `Daily check at 19:30`
- `Stand up at 7:00AM every day`
- `Hydrate in 10m every 2 hours`
- `Reminder at 7:00AM every Saturday, etc.`
- `Workout at 6:30AM every Mon-Fri`
- `Plan sprint at 8:15AM every weekdays`

## User Data Storage

Reminders and user-editable greetings are stored in Documents:

- `QStandardPaths::DocumentsLocation/RemindMe/reminders.json`
- `QStandardPaths::DocumentsLocation/RemindMe/greetings.txt`

On startup, RemindMe automatically migrates existing legacy data from:

- `QStandardPaths::AppDataLocation/reminders.json`
- legacy app-folder `greetings.txt` (when present) into Documents storage

If `greetings.txt` is deleted, RemindMe regenerates a fresh default file automatically.

## Inspiration

RemindMe is inspired by:

- Tatsumaki Bot's `t!remind` feature
- Free Countdown Timer

This project is an independent work and is not affiliated with or endorsed by those products.

## Install

- **Windows:** download `RemindMe-<version>-setup.exe` (or the portable `.zip`) from [Releases](https://github.com/canikou/RemindMe/releases). Upgrading from 1.x keeps your reminders.
- **Linux:** download `RemindMe-<version>-x86_64.AppImage`, make it executable (`chmod +x`), and run it. On first run it adds itself to your app menu. Updates replace the AppImage in place.

Linux notes:

- The tray icon needs a StatusNotifierItem tray. KDE, Cinnamon, XFCE and most others have one; on GNOME enable the *AppIndicator and KStatusNotifierItem Support* extension. Without a tray, closing the window quits the app.
- RemindMe runs through XWayland on Wayland sessions, so the overlay can stay on top and due popups can come to the front (native Wayland allows neither).

## Development

Requirements: a stable Rust toolchain (`rustup`). Nothing else on Windows or Linux.

```bash
cargo run              # debug build of the app
cargo test             # parser, scheduling, storage and update-rule tests
cargo clippy --all-targets -- -D warnings
cargo fmt
```

Useful environment variables:

- `REMINDME_THEME=dark|light` forces a theme.
- `REMINDME_REDUCE_MOTION=1` settles the split-flap tiles (for screenshots).
- `REMINDME_DEBUG=1` logs UI messages to stderr.

### Packaging

- **Windows portable + setup.exe** (needs [Inno Setup 6](https://jrsoftware.org/isinfo.php)):
  ```powershell
  cargo build --release
  ./scripts/package-portable-release.ps1   # dist/RemindMe-<version>-windows-portable(.zip)
  ./scripts/smoke-test-package.ps1 -PackageDir dist/RemindMe-<version>-windows-portable
  ./scripts/package-setup-release.ps1      # dist/RemindMe-<version>-setup.exe
  ```
- **Linux AppImage** (downloads `linuxdeploy` into `.tools/`):
  ```bash
  cargo build --release
  ./scripts/package-appimage.sh            # dist/RemindMe-<version>-x86_64.AppImage
  ```

CI (`.github/workflows/ci.yml`) builds, tests and packages both platforms, smoke-tests the packages, and uploads them as artifacts. The Linux AppImage is built on Ubuntu 22.04, so it runs on that glibc or newer.

### Auto-Update Release Contract (GitHub)

RemindMe checks GitHub `releases/latest` at startup (at most once per day) and from the tray or More menu.

- Tag releases with a three-part semver tag like `v2.0.0`.
- Attach `RemindMe-<version>-setup.exe` (Windows; 1.x clients also use it), `RemindMe-<version>-windows-portable.zip`, and `RemindMe-<version>-x86_64.AppImage` (Linux).
- Linux asset names must not end in `.exe`, `.msi` or `.zip`, so Windows clients never pick them.

### Project Structure

- `src/lib.rs` and modules: the UI-free core (`parser`, `schedule`, `store`, `weekday`, `time_format`, `greetings`, `update`).
- `src/main.rs`, `src/app.rs`: the iced app (main window, overlay, due popups, dialogs); `src/theme.rs`, `src/flap.rs` for the look.
- `src/platform.rs`: tray, single instance, AppImage desktop integration; `src/updater.rs`: download and install.
- `tests/core_tests.rs`: tests, including files and share strings written by 1.x.
- `installer/RemindMe.iss`, `scripts/`: Windows and Linux packaging.

### Contributing and Security

- Contribution guide: `CONTRIBUTING.md`
- Security policy: `SECURITY.md`
