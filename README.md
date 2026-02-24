# RemindMe

RemindMe is a Windows desktop reminders app built with C++23 and Qt6 Widgets.
Current version: `v1.2.1`.

## User Experience

- Multiple reminders sorted by soonest due time.
- Natural-language parsing for relative reminders (`in ...`) and time-of-day reminders (`at ...`).
- Repeat scheduling via interval syntax (`every 2 hours`) and weekday syntax (`every weekdays`, `every Mon-Fri`, `every Monday through Friday`).
- Integer math support in duration input (for example `(3*10) minutes`).
- Repeating reminder subtasks with progress tracking and per-cycle reset.
- Popup notifications with `Snooze (5 min.)` and `OK`.
- Closing a reminder popup with `X` behaves the same as pressing `OK`.
- Single-instance guard prevents duplicate app processes.
- System tray runtime behavior keeps timers active when the main window is closed.
- Optional always-on-top compact overlay with drag support and guarded title/timer layout to avoid clipping.
- Completed reminder history with quick re-add and a collapsible slide in/slide out preview in the main window.
- Import/export share-string flow for moving reminders between instances.
- Persistent JSON storage in Documents with automatic one-time migration from legacy app-data location.

## Input Examples

- `Drink water in 45m`
- `Stretch in 2h 30m`
- `Hydrate in (3*10) minutes`
- `Stand up at 7:00AM`
- `Daily check at 19:30`
- `Stand up at 7:00AM every day`
- `Hydrate in 10m every 2 hours`
- `Reminder at 7:00AM every Saturday, etc.`
- `Workout at 6:30AM every Mon-Fri`
- `Plan sprint at 8:15AM every weekdays`

## User Data Storage

Reminders are stored at:

- `QStandardPaths::DocumentsLocation/RemindMe/reminders.json`

On startup, RemindMe automatically migrates existing legacy data from:

- `QStandardPaths::AppDataLocation/reminders.json`

## Inspiration

RemindMe is inspired by:

- Tatsumaki Bot's `t!remind` feature
- Free Countdown Timer

This project is an independent work and is not affiliated with or endorsed by those products.

## Technical / Development

This repository follows the `cpp23-mntchocoluvr` standardized layout and workflow (presets, VS Code tasks, CTest, CI, clang targets).

### Prerequisites

- Windows with MSYS2 UCRT64 at `C:/msys64/ucrt64`
- Required packages:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-qt6-tools
```

### Standard Workflow (Presets + CTest)

Use presets first:

1. Configure debug:
   - `cmake --preset debug`
2. Build debug:
   - `cmake --build --preset debug --parallel`
3. Run tests:
   - `ctest --preset debug` 

Convention gate:

- `./scripts/check-conventions.ps1`
- CI also runs this check before build/test.
- CMake target equivalent: `cmake --build --preset debug --target conventions`
- Additional build helper targets:
  - `cmake --build --preset debug --target format-check`
  - `cmake --build --preset debug --target lint`

Release:

1. Configure release:
   - `cmake --preset release`
2. Build release:
   - `cmake --build --preset release --parallel`
3. Run tests:
   - `ctest --preset release`

### Portable Release Packaging (Windows)

1. Ensure release binary is built:
   - `cmake --preset release`
   - `cmake --build --preset release --parallel`
2. Create portable folder + zip:
   - `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/package-portable-release.ps1`

Output:

- `dist/RemindMe-<version>-windows-portable/`
- `dist/RemindMe-<version>-windows-portable.zip`

### VS Code Workflow

- `Ctrl+Shift+B`: default task `build (debug)`
- `F5`: launch profile `Debug (Preset Debug Binary)`
- Additional tasks:
  - `configure (debug)` / `configure (release)`
  - `build (debug)` / `build (release)`
  - `test (debug)` / `test (release)`
  - `format-check (debug)` / `lint (debug)`

### Project Structure

- `CMakeLists.txt`: template-aligned main build configuration
- `CMakePresets.json`: local + CI presets (`debug`, `release`, `ci-debug`, `ci-release`, `ci-lint`)
- `src/`: implementation files in `snake_case` (for example `main_window.cpp`)
- `include/remindme/`: public project headers in `snake_case` (for example `main_window.hpp`)
- `resources/`: app resources/icons
- `scripts/package-portable-release.ps1`: creates portable Windows release folder + zip
- `tests/core_tests.cpp`: baseline tests registered with CTest
- `.vscode/`: standardized tasks, launch, settings, snippets
- `.github/workflows/ci.yml`: CI for debug/release build and tests

### Naming Conventions

- Project code is scoped under `namespace remindme`.
- Project headers use `.hpp` and `snake_case` filenames.
- Source files use `snake_case` filenames.

### Binary Naming Convention

- Debug builds output `app.exe`
- Release builds output `RemindMe.exe`

This keeps debug launch paths stable while preserving release naming.

### Contributing and Security

- Contribution guide: `CONTRIBUTING.md`
- Security policy: `SECURITY.md`
