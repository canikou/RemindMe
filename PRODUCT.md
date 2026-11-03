# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

<!-- Not a web app: RemindMe is a native desktop app for Windows and Linux (Rust, iced). "web" is the schema's closest value; the design language is desktop, not iOS/Android. -->

## Stack

Rust + iced (since 2.0, chosen by the owner for Windows + Linux support). Windows ships an Inno Setup installer and a portable zip; Linux ships an AppImage. Saved reminders, share strings and auto-update from 1.x must keep working.

## Users

General Windows and Linux desktop users, including non-technical people who will not learn an input syntax up front. They keep the PC on for long stretches (work, study, gaming) and want to be reminded of things without managing a to-do app.

## Product Purpose

Set a reminder in seconds, see what's coming at a glance, and get interrupted reliably when it's due. Success means: a new user adds their first reminder correctly without reading docs, can always tell what fires next, and never misses or loses a reminder.

## Positioning

One-line natural-language capture ("Drink water in 45m", "Stand up at 7:00AM every weekday", "Dentist on October 24, 2026 6:30AM") instead of forms and date pickers. Time math is supported in input (`(3*10) minutes`). Countdowns are first-class (inspired by Free Countdown Timer and Tatsumaki Bot's `t!remind`), and there's an always-on-top overlay for glancing at what's left.

## Operating Context

- Runs all day from the system tray. Closing the window keeps timers running, and Quit is in the tray menu.
- The optional overlay sits always-on-top over other apps (games, editors) and can be dragged.
- When a reminder is due, a popup comes to the front and is acted on with Done or Snooze 5 min. Esc snoozes; closing with X counts as done.
- Data lives in `Documents/RemindMe/reminders.json`. Greetings live in an editable `Documents/RemindMe/greetings.txt`.
- Reminders move between machines with export/import share strings.
- Updates come from GitHub Releases via the in-app updater, preferring `setup.exe` on Windows and the AppImage on Linux.

## Capabilities and Constraints

- Schedule kinds: relative (`in …`), time of day (`at …`), specific date (`on <date> <time>`), repeat by interval (`every 2 hours`) or by weekdays (`every Mon-Fri`).
- Subtasks exist only on repeating reminders and reset each period.
- The history of completed one-time reminders keeps the last 50 and supports Add Again.
- Single instance. Windows and Linux (X11, or XWayland on Wayland sessions).
- Must keep: typed natural-language input as the primary way to add reminders (user-confirmed). Supporting UI (preview, examples, a structured fallback) is allowed.
- Must keep as features, but open to redesign in look and placement: overlay, tray, greetings, history.
- The saved JSON format and the update/installer contract must stay backward compatible.

## Evidence on Hand

- No testimonials, user counts or press. Do not invent any.
- App icon: `resources/icons/app.ico`, `resources/icons/app.png`.
- Default greetings: `greetings.txt` and built-in pools in `src/greetings.rs`.

## Product Principles

1. **Typing is the fast path, never a guessing game.** Show what the app understood before it commits, and teach the syntax in place.
2. **Next reminder first.** The soonest reminder and its countdown are always the most prominent thing.
3. **Never lose a reminder.** Destructive or ambiguous actions are confirmed or reversible, and no dismissal path leaves a reminder stuck.
4. **Quiet until it matters.** The resting UI is calm and glanceable; the due popup is the one loud moment.
5. **Friendly, not cute.** Greetings add warmth, but controls use plain words.
