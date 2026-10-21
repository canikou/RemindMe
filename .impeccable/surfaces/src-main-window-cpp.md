---
version: 1
slug: "src-main-window-cpp"
primary_target: "src/main_window.cpp"
related_targets: ["src/reminder_popup.cpp"]
---

# Main window, popup and overlay

Scope: RemindMe's main window (`src/main_window.cpp`), due popup (`src/reminder_popup.cpp`) and always-on-top overlay. Mode: Operate. Build path: code-led.

Audience and job: general Windows users, including non-technical ones. Two uses, about equally common: open from the tray, type one line, glance and close; or leave the window open all day. Success: the next reminder and its countdown are unmistakable, a first-time user adds a correct reminder without docs, and nothing gets lost.

Constraints: Qt6 Widgets, Windows only. Follows the Windows light/dark setting. Typed natural-language input stays primary. Overlay, tray, greetings and history stay but can be reshaped. Avoid: corporate to-do app, dense or technical.

## Direction contract

THESIS: Reminders read as a departures board. The next one leaves first, its countdown is the biggest thing on screen, and the rest wait in timetable rows. Refuses the category default: a grey list of checkbox cards with an accent button.

OWN-WORLD: A deep navy board band (#16213A) on a cool light ground (#F3F4F7), or a near-black blue ground (#10141C) in dark mode. White or ink rows on hairline rules. Amber (#F5C84B; #8A6100 as text on light) is used only for the next countdown and "Due now". Segoe UI throughout with tabular figures. Small caps-tracked column headers. 8px corners.

STORY: The user sees what leaves next and how long until it does. They type one line, read "Understood as …" as they type, press Enter or Add, and the row lands in its slot. When it's due, the popup is a single board card: Done or Snooze 5 min.

FIRST VIEWPORT: A full-width navy board on top. Greeting and clock sit small top left, with Overlay, History and More on the right. Below them, "NEXT REMINDER" in caps amber, the title at 26px, the when/repeat line, and the countdown at 56px amber on the right. Under the board, a white departure table with columns Time, Reminder, Repeats and Due in, and per-row Edit/Delete. The input is docked at the bottom with a navy Add button and a live preview line.

FORM: Departures Board, position 1 on the ordered list (picked as IMPECCABLE'S PICK). seed:b65d998b77581962

FINISH: unreviewed and undocumented is unfinished; this build ends with the finish review, the verdict, DESIGN.md, and every shipping raster carrying its provenance

Signature move: the board header's NEXT REMINDER with an amber countdown. The overlay repeats it as a mini board, and a row reaching zero flips its Due-in cell to amber "Due now".
