# RemindMe

RemindMe is a Windows desktop reminders app built with C++ and Qt6 Widgets.
It focuses on fast text-based reminder entry and persistent local scheduling.

## Features

- Multiple reminders, sorted by soonest due time
- Text parsing for:
  - relative reminders (`in ...`)
  - time-of-day reminders (`at ...`)
  - repeating directives (`every ...`)
- Repeating and non-repeating reminders
- Popup notifications with:
  - `Snooze (5 min.)`
  - `OK` acknowledge
- Foreground/focus assist on reminder popup
- Edit and delete reminder actions
- Completed reminder history (non-repeating reminders), with quick re-add
- Persistent JSON storage via Qt standard app-data location
- Editable greeting message bank via `greetings.txt`

## Input Examples

- `Drink water in 45m`
- `Stretch in 2h 30m`
- `Stand up at 7:00AM`
- `Daily check at 19:30`
- `Stand up at 7:00AM every day`
- `Hydrate in 10m every 2 hours`

## Greeting Messages

- Greeting text is loaded from `greetings.txt`.
- Supported sections:
  - `[morning]`
  - `[afternoon]`
  - `[evening]`
- If `greetings.txt` is missing, the app auto-creates it with default messages.
- Restart the app after editing `greetings.txt` to apply updates.

## Data Storage

Reminders are stored at:

- `QStandardPaths::AppDataLocation/reminders.json`

On Windows this resolves under the current user profile app-data directory.

## Build (MSYS2 UCRT64)

Run from an **MSYS2 UCRT64** shell.

### 1) Install dependencies

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-qt6-tools
```

### 2) Configure and build

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build build
```

### 3) Run

```powershell
.\build\app.exe
```

## Deploy / Share Build

For a portable build, collect required Qt runtime files:

```powershell
windeployqt6 .\build\app.exe
```

Share `app.exe` with all deployed Qt DLLs/platform plugins.

## Inspiration

RemindMe is inspired by:

- Tatsumaki Bot's `t!remind` feature
- Free Countdown Timer

This project is an independent work and is **not affiliated with or endorsed by** those products.
