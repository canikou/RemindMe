# Changelog

## 1.0.0 - 2026-02-17

### Initial Release

- Released RemindMe as a Qt6 desktop reminders app for Windows (MSYS2 UCRT64 build target).
- Added robust reminder flow: parsing (`in` / `at` / `every`), sorting by soonest, repeat handling, edit/delete, and 5-minute snooze popups.
- Added persistent storage for active reminders and completed reminder history with re-add support.
- Added safety polish for very short repeating intervals and completion history de-duplication with occurrence counts.
- Added editable greeting message support through external `greetings.txt`, including auto-create when missing.
- Unified shared time-format helpers and reduced per-tick UI work by updating countdown labels without full list rebuilds.
- Improved save/load resilience, cleaned code comments/includes, and refreshed project hygiene/docs for public sharing.
