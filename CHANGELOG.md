# Changelog

## Unreleased

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
