# RemindMe

RemindMe is a Windows desktop reminders app built with C++20 and Qt6 Widgets.
Current version: `v1.1.0`.

## What It Does

- Multiple reminders, sorted by soonest due time
- Text parsing for:
  - relative reminders (`in ...`)
  - time-of-day reminders (`at ...`)
  - repeating directives (`every ...`)
- Repeating and non-repeating reminders
- Popup notifications with:
  - `Snooze (5 min.)`
  - `OK` acknowledge
- Closing a reminder popup window (`X`) behaves the same as pressing `OK`
- System tray behavior:
  - Closing main window hides to tray
  - Timers/reminders continue running in background
- Edit/delete reminders
- Completed reminder history with quick re-add
- Persistent JSON storage via Qt standard app-data location

## Input Examples

- `Drink water in 45m`
- `Stretch in 2h 30m`
- `Stand up at 7:00AM`
- `Daily check at 19:30`
- `Stand up at 7:00AM every day`
- `Hydrate in 10m every 2 hours`

## Data Storage

Reminders are stored at:

- `QStandardPaths::AppDataLocation/reminders.json`

## Inspiration

RemindMe is inspired by:

- Tatsumaki Bot's `t!remind` feature
- Free Countdown Timer

This project is an independent work and is not affiliated with or endorsed by those products.

## Development

This repository follows the `cpp-mntchocoluvr` standardized layout and workflow (presets, VS Code tasks, CTest, CI).

### Prerequisites

- Windows with MSYS2 UCRT64 tools available on `PATH`
- Required packages:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-qt6-tools
```

The shared presets are path-agnostic. If Qt is not discoverable from `PATH`, set `CMAKE_PREFIX_PATH` or add a local, untracked `CMakeUserPresets.json`.

For repository maintenance, prefer the installed CLI helpers: `rg` for search, `fd` for file discovery, `jq` for JSON, and `gh` for GitHub operations.

### Standard Workflow

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

Release:

1. Configure release:
   - `cmake --preset release`
2. Build release:
   - `cmake --build --preset release --parallel`
3. Run tests:
   - `ctest --preset release`

### VS Code Workflow

- `Ctrl+Shift+B`: default task `build (debug)`
- `F5`: launch profile `Debug (Preset Debug Binary)`
- Additional tasks:
  - `configure (debug)` / `configure (release)`
  - `build (debug)` / `build (release)`
  - `test (debug)` / `test (release)`

### Project Structure

- `CMakeLists.txt`: template-aligned main build configuration
- `CMakePresets.json`: `debug` and `release` presets
- `src/`: implementation files in `snake_case` (for example `main_window.cpp`)
- `include/remindme/`: public project headers in `snake_case` (for example `main_window.hpp`)
- `resources/`: app resources/icons
- `tests/core_tests.cpp`: baseline tests registered with CTest
- `.vscode/`: standardized tasks, launch, settings, snippets
- `.github/workflows/ci.yml`: CI for debug/release build and debug tests

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
