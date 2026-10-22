# Changelog

## Unreleased

### Fixes

- The split-flap flip no longer blanks the top half of a tile mid-flip, which made the new digit pop in at the end.

## 1.3.2 - 2026-10-23

### Changes

- The split-flap countdown now flips even when Windows "Show animations" is turned off. It is the app's only animation.

## 1.3.1 - 2026-10-22

### Fixes

- Closing a reminder popup while the window was in the tray quit RemindMe. If that shutdown then stalled, the app was left running with no window, and every relaunch said "already running". RemindMe now only quits from the tray's **Quit**, or when the window closes and there is no tray.
- The single-instance lock is released as soon as the app exits.
- Opening RemindMe while it's already running now brings up its window instead of showing a message. If the running copy doesn't respond, you're offered to end it and start again.

## 1.3.0 - 2026-10-22

### Redesign: departures board

- New look built like a departures board: a navy board up top shows the next reminder and a large amber countdown, with the rest listed below in Time / Reminder / Repeats / Due in columns.
- Follows the Windows light/dark app setting, and switches live when it changes.
- As you type, a live "Understood as …" line shows what will be scheduled and when, before you press Enter.
- Export, Import and Check for updates moved into a **More** menu on the board. Overlay and History stay one click away.
- The due popup is now a board card ("Due today at 1:22 PM") with **Done** and **Snooze 5 min**.
- The overlay matches the board, and the next reminder's countdown is highlighted.
- "Recently done" starts collapsed so the list gets the room.
- Rows reaching zero show **Due now**.
- The board countdown is a split-flap display: each digit is a tile that flips when it changes. The flip is skipped when Windows animations are turned off.
- The board announces like a station: "Now boarding: …" within 15 minutes, "Final call: …" when due.
- Each row has a journey line that fills as the reminder gets closer (one shared 6-hour scale), amber for the next one.

### Technical

- New `Theme` module (`src/theme.cpp`) holds both palettes and the app-wide style sheet; widgets use `role` properties instead of inline colours.
- CI renders the main window, overlay and popup in both themes (`RemindMe_ui_screenshots`) and uploads them as a build artifact.

## 1.2.4 - 2026-10-21

### User Experience

- Parse errors now show inline under the input instead of a blocking dialog, and there's a visible **Add** button.
- Deleting a reminder asks for confirmation first.
- Pressing Esc on a reminder popup now snoozes it. Previously the popup hid without signalling, which left that reminder stuck until restart.
- Popup buttons now read **Snooze 5 min** / **Done**. Done is the default button, long titles wrap, and keyboard focus is visible again.
- Edit dialog keeps the date of one-shot "at" reminders (it used to reset them to the next occurrence of the time). It also validates before closing so a typo no longer discards the other edits.
- Completed one-shot "at" reminders no longer show a misleading "daily" pattern.
- Non-repeating reminders no longer show a "(None)" line. The empty state lists example inputs.
- Buttons and inputs have focus, pressed and disabled states.

## 1.2.3 - 2026-10-21

### Features

- Reminders can now be set for a specific date: `Reinforce on October 24, 2026 6:30AM`, `Pay rent at 6:30PM on 2026-11-05`, `Party at 5 Dec 8pm`. Dates without a year pick the next occurrence. Past dates, impossible dates, missing times and combining a date with `every ...` are rejected with a clear message.

### Fixes

- Portable package and `setup.exe` now bundle the MinGW runtime (`libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll`) and Qt's third-party DLLs (ICU, PCRE2, zlib, zstd, HarfBuzz, FreeType, ...). Previously `windeployqt` shipped only Qt modules, so the installed app failed to start with missing-DLL errors on machines without MSYS2.
- Packaging now fails if any non-system DLL dependency is left unresolved.
- CI now builds the portable zip and `setup.exe` on every push, silently installs the setup, and smoke-launches both with a system-only `PATH` to catch missing DLLs before release.

## 1.2.2 - 2026-02-24

### User Experience

- Custom `greetings.txt` is now preserved more safely across app updates by migrating it into Documents storage (`Documents/RemindMe/greetings.txt`) when needed.
- If `greetings.txt` is deleted, RemindMe auto-regenerates a default greetings file so users can quickly reset from corruption/customization issues.
- RemindMe can now check GitHub Releases for newer versions at startup and, with user consent, download and launch installer assets automatically when available.
- Windows installer (`setup.exe`) packaging is now documented and standardized as the primary path for seamless in-app auto-updates.

### Technical

- Greeting-file resolution now prefers Documents storage with a legacy migration path and an initialization marker to support explicit delete-to-reset behavior.
- Added a release-updater client flow that reads GitHub release metadata, compares semantic versions, validates optional SHA-256 digests, and supports installer launch (`.exe`/`.msi`) after download.
- Added installer packaging assets (`installer/RemindMe.iss`, `scripts/package-setup-release.ps1`) to produce versioned `dist/RemindMe-<version>-setup.exe` artifacts for releases.
- Extracted updater version/asset selection logic into `update_utils` and added core tests that lock in semver comparison and release-asset selection behavior.

## 1.2.1 - 2026-02-24

### User Experience

- Weekday-aware repeating schedule parsing for `every ...` on `at` reminders, including single days (`every Saturday`), aliases (`weekdays`, `weekends`), multi-day expressions (`Mon Wed Fri`), range forms (`Mon-Fri`, `Monday through Friday`, `Tue thru Thu`, `from Monday to Friday`), and tolerant punctuation/variants such as `etc.` and Unicode dash separators.
- Main-window completed-history preview supports click-to-toggle slide in/slide out collapse (`Show`/`Hide`) to reclaim space for active reminders.
- Overlay window (introduced in `1.2.0`) is now draggable (click-and-drag across overlay content) with system move fallback where available.
- Overlay row content (introduced in `1.2.0`) no longer drifts horizontally over time and no longer clips timer text off-window during normal resizing/refresh cycles.

### Technical

- Portable Windows release packaging script (`scripts/package-portable-release.ps1`) stages `RemindMe.exe`, runs `windeployqt`, and creates a portable zip under `dist/`.
- Expanded edge-case tests in `tests/core_tests.cpp` for parser time boundaries, duration math precedence, weekday token separators, store-load corruption handling, completed-history capping, and plain-JSON import ID regeneration.
- Overlay layout internals were reworked to preserve the current look while improving stability (frameless/translucent container, explicit width budgeting with 65/35 title/time targeting, and narrower-width fallback fitting logic).
- CI now sets MSYS2 as the default shell for configure/build/test steps while keeping the convention check on PowerShell.
- Documentation was refreshed to reflect current parser, overlay, completed-preview, and packaging capabilities.

## 1.2.0 - 2026-02-23

### User Experience

- Single-instance lock at startup prevents multiple RemindMe processes from running simultaneously.
- First release with overlay support: added a toggle-able always-on-top overlay for compact remaining-task display, including subtask progress when present.
- Due reminders advance through popup queues more reliably with one-active-popup sequencing and stronger foreground refocus for each queued reminder.
- Startup migration now automatically carries existing legacy reminder data into the new Documents location before load.

### Technical

- Build baseline migrated to C++23.
- Adopted updated template conventions (`cmake/ProjectOptions.cmake`, `cmake/ClangTools.cmake`, CI preset naming, and lint/format targets).
- Reminder storage default moved to `Documents/RemindMe/reminders.json`.
- Expanded parser and reminder-store persistence/migration tests.

## 1.1.1 - 2026-02-20

### User Experience

- Repeating reminder subtasks were added with per-period reset and inline quick-complete toggles in the reminder list.
- Import/export share-string flow was added for moving reminders between instances.
- Relative duration input now supports integer math expressions (for example `(3*10) minutes`).
- Subtask progress is shown on each repeating reminder row, with completed subtasks hidden from the quick preview.
- Reminder popup focus behavior on startup was improved so due popups come to the front more reliably.

### Technical

- Due reminder popups now queue sequentially to avoid concurrent popup windows.

## 1.1.0 - 2026-02-19

### User Experience

- System tray runtime mode was added: closing the main window hides to tray while reminders/timers continue running.
- Reminder popup close button (`X`) now follows the same completion path as `OK`.

### Technical

- Added template-style repository workflow and standards (CMake presets, VS Code task/launch alignment, CI workflow/policy files, convention checks, and baseline CTest integration).
- Migrated project layout to standardized structure (`include/remindme/*.hpp`, `snake_case` source names, and `namespace remindme` scoping).
- README organization was updated to keep user-facing usage ahead of development details.

## 1.0.0 - 2026-02-17

### User Experience

- Initial release of RemindMe as a Qt6 desktop reminders app for Windows (MSYS2 UCRT64 target).
- Core reminder flow shipped with parsing (`in` / `at` / `every`), soonest-first sorting, repeat handling, edit/delete, and 5-minute snooze popups.
- Persistent storage shipped for active reminders and completed reminder history with re-add support.
- Added safeguards for very short repeating intervals and completion-history de-duplication with occurrence counts.
- Added editable greeting message support through external `greetings.txt`, including auto-create when missing.

### Technical

- Unified shared time-format helpers and reduced per-tick UI work by updating countdown labels without full list rebuilds.
- Improved save/load resilience and refreshed code comments/includes and repository hygiene/docs for public sharing.
