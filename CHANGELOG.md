# Changelog

## 1.1.0 - 2026-02-19

### Added

- System tray runtime mode: closing the main window now hides to tray while reminders/timers continue running.
- Reminder popup close button (`X`) now follows the same completion path as `OK`.
- Template-style repository workflow and standards:
  - CMake presets and VS Code task/launch alignment
  - CI workflow and repository policy files
  - Convention checks (`scripts/check-conventions.ps1`) and CMake `conventions` target
  - Baseline test target with CTest integration

### Changed

- Migrated project layout to standardized structure:
  - headers moved to `include/remindme/*.hpp`
  - source files renamed to `snake_case`
  - project symbols scoped under `namespace remindme`
- Updated README organization to prioritize user-facing usage, with development details moved lower.

## 1.0.0 - 2026-02-17

### Initial Release

- Released RemindMe as a Qt6 desktop reminders app for Windows (MSYS2 UCRT64 build target).
- Added robust reminder flow: parsing (`in` / `at` / `every`), sorting by soonest, repeat handling, edit/delete, and 5-minute snooze popups.
- Added persistent storage for active reminders and completed reminder history with re-add support.
- Added safety polish for very short repeating intervals and completion history de-duplication with occurrence counts.
- Added editable greeting message support through external `greetings.txt`, including auto-create when missing.
- Unified shared time-format helpers and reduced per-tick UI work by updating countdown labels without full list rebuilds.
- Improved save/load resilience, cleaned code comments/includes, and refreshed project hygiene/docs for public sharing.
