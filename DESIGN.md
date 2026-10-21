---
name: RemindMe
description: A departures board for reminders. The next one leaves first, and its countdown is the loudest thing on screen.
colors:
  ground: "#F3F4F7"
  surface: "#FFFFFF"
  surface-alt: "#F7F8FA"
  border: "#DDE1E8"
  rule: "#ECEEF2"
  text: "#16213A"
  muted: "#566075"
  board: "#16213A"
  board-raised: "#22305A"
  board-text: "#FFFFFF"
  board-muted: "#B4BED2"
  accent: "#F5C84B"
  due-text: "#8A6100"
  danger: "#B3261E"
  focus: "#3B6FD8"
  primary: "#16213A"
  primary-hover: "#22305A"
  primary-text: "#FFFFFF"
  ground-dark: "#10141C"
  surface-dark: "#171C26"
  surface-alt-dark: "#1C2230"
  border-dark: "#2C3446"
  rule-dark: "#232A39"
  text-dark: "#E8ECF4"
  muted-dark: "#A3ACBE"
  board-dark: "#1B2742"
  board-raised-dark: "#26355A"
  due-text-dark: "#F5C84B"
  danger-dark: "#FF8A80"
  focus-dark: "#7EA6FF"
  primary-dark: "#2B3D66"
  primary-hover-dark: "#364B7D"
typography:
  display:
    fontFamily: "Segoe UI Variable Text, Segoe UI"
    fontSize: "40pt"
    fontWeight: 600
  display-idle:
    fontFamily: "Segoe UI Variable Text, Segoe UI"
    fontSize: "40pt"
    fontWeight: 300
  headline:
    fontFamily: "Segoe UI Variable Text, Segoe UI"
    fontSize: "22pt"
    fontWeight: 600
  headline-due:
    fontFamily: "Segoe UI Variable Text, Segoe UI"
    fontSize: "20pt"
    fontWeight: 600
  title:
    fontFamily: "Segoe UI Variable Text, Segoe UI"
    fontSize: "19pt"
    fontWeight: 600
  cell-title:
    fontFamily: "Segoe UI Variable Text, Segoe UI"
    fontSize: "11pt"
    fontWeight: 600
  cell-figure:
    fontFamily: "Segoe UI Variable Text, Segoe UI"
    fontSize: "11pt"
    fontWeight: 400
  body:
    fontFamily: "Segoe UI Variable Text, Segoe UI"
    fontSize: "10pt"
    fontWeight: 400
  hint:
    fontFamily: "Segoe UI Variable Text, Segoe UI"
    fontSize: "9pt"
    fontWeight: 400
  label:
    fontFamily: "Segoe UI Variable Text, Segoe UI"
    fontSize: "8pt"
    fontWeight: 700
    letterSpacing: "108%"
  overlay-figure-next:
    fontFamily: "Segoe UI Variable Text, Segoe UI"
    fontSize: "21px"
    fontWeight: 700
  overlay-figure:
    fontFamily: "Segoe UI Variable Text, Segoe UI"
    fontSize: "15px"
    fontWeight: 700
rounded:
  check: "3px"
  item: "4px"
  control: "6px"
  table: "8px"
  overlay: "10px"
spacing:
  hair: "2px"
  xs: "4px"
  sm: "8px"
  md: "12px"
  lg: "16px"
  xl: "20px"
  board: "24px"
  popup: "28px"
components:
  button:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.text}"
    rounded: "{rounded.control}"
    padding: "7px 14px"
  button-hover:
    backgroundColor: "{colors.surface-alt}"
  button-pressed:
    backgroundColor: "{colors.rule}"
  button-primary:
    backgroundColor: "{colors.primary}"
    textColor: "{colors.primary-text}"
    rounded: "{rounded.control}"
    padding: "8px 20px"
  button-primary-hover:
    backgroundColor: "{colors.primary-hover}"
  button-row-action:
    backgroundColor: "transparent"
    textColor: "{colors.muted}"
    rounded: "{rounded.control}"
    padding: "5px 10px"
  button-row-action-hover:
    backgroundColor: "{colors.surface-alt}"
    textColor: "{colors.text}"
  button-row-danger-hover:
    backgroundColor: "{colors.surface-alt}"
    textColor: "{colors.danger}"
  button-board:
    backgroundColor: "transparent"
    textColor: "{colors.board-text}"
    rounded: "{rounded.control}"
    padding: "6px 12px"
  button-board-hover:
    backgroundColor: "{colors.board-raised}"
  input:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.text}"
    rounded: "{rounded.control}"
    padding: "7px 10px"
  board-header:
    backgroundColor: "{colors.board}"
    textColor: "{colors.board-text}"
    padding: "14px 20px 22px 24px"
  departure-table:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.text}"
    rounded: "{rounded.table}"
  departure-row:
    padding: "12px 8px 12px 16px"
  overlay-panel:
    backgroundColor: "{colors.board}"
    textColor: "{colors.board-text}"
    rounded: "{rounded.overlay}"
    padding: "2px 14px"
    width: "280px"
  popup-board:
    backgroundColor: "{colors.board}"
    textColor: "{colors.board-text}"
    padding: "22px 28px 24px 28px"
  checkbox:
    backgroundColor: "{colors.surface}"
    rounded: "{rounded.check}"
    size: "14px"
  checkbox-checked:
    backgroundColor: "{colors.text}"
---

# Design System: RemindMe

## Overview

**Creative North Star: "The Departures Board"**

RemindMe reads like a station timetable. A deep navy board band carries the next departure: its title, its when/repeat line, and an amber countdown that is the biggest thing on screen. Below it, the rest of the reminders wait in a plain white (or ink) timetable with fixed columns for Time, Reminder, Repeats and Due in, separated by hairline rules. The same board material reappears twice, and only twice: as the always-on-top overlay panel and as the head of the due popup.

The world is calm at rest and loud only at the moment of departure. Density is moderate, set by a fixed column grid rather than by cards. Everything is flat: depth comes from the one navy mass against a cool light ground (or a near-black blue ground in dark mode), never from shadows. Typography is Segoe UI throughout, so the app sits natively on Windows; hierarchy is carried by size and weight alone.

This is a native Qt6 Widgets app, not a web page. Every token below lives in `src/theme.cpp` as a `ThemePalette` field (declared in `include/remindme/theme.hpp`) and is substituted into one app-wide Qt style sheet through `@token` placeholders. Widgets opt into a style through a dynamic `role` property (`QLabel[role="cellDue"]`) or an `objectName` (`QFrame#board`, `QPushButton#primaryButton`). The palette follows the Windows `AppsUseLightTheme` registry value, with `REMINDME_THEME=light|dark` as an override.

**Key Characteristics:**
- One navy board band per surface, carrying the next reminder.
- Amber marks the next countdown and "Due now", nothing else.
- Timetable rows on 1px hairline rules, with fixed-width columns.
- Flat: no shadows anywhere; depth is tonal.
- Segoe UI only; size and weight carry hierarchy.
- Two complete palettes (light and dark) with identical roles.

## Colors

A cool, low-chroma navy-and-paper palette with a single warm signal colour.

### Primary
- **Departure Board Navy** (`board`; `board-dark` in dark mode): the board header band, the overlay panel and the popup's board card. In light mode it is the same value as `text` and `primary`, so the board, the ink and the Add button are one colour.
- **Raised Board Navy** (`board-raised`): hover and checked state for buttons on the board, the overlay panel's 1px border, and the rules between overlay rows.
- **Primary Navy** (`primary`, hover `primary-hover`): the one filled button per surface (`QPushButton#primaryButton`: Add, Done). In dark mode it lifts to `primary-dark` so it separates from the dark ground.

### Secondary
- **Departure Amber** (`accent`): the next reminder's countdown on the board, the next row's countdown in the overlay. Same value in both modes, and only ever placed on navy.
- **Due Amber** (`due-text`; `due-text-dark`): "Due now" in a timetable row's Due-in cell (`cellDue`) and the due line in the popup body (`popupDue`). On the light ground amber darkens to a readable bronze; in dark mode it is the same amber as `accent`.

### Tertiary
- **Signal Red** (`danger`; `danger-dark`): parse errors under the input (`role="error"`) and the hover/focus text of a row's Delete action. Never a fill.
- **Focus Blue** (`focus`; `focus-dark`): 1px focus border on inputs and buttons, checkbox hover border, and text selection background. On the board, focus switches to amber instead.

### Neutral
- **Cool Ground** (`ground`; `ground-dark`): window, dialog and popup background.
- **Paper** (`surface`; `surface-dark`): the departure table, inputs, default buttons, menus, tooltips, unchecked checkboxes.
- **Paper Hover** (`surface-alt`; `surface-alt-dark`): hover fill for default buttons, row actions and menu items; combo-box selection.
- **Edge** (`border`; `border-dark`): 1px outline of inputs, buttons, the table and menus; scrollbar handle.
- **Hairline** (`rule`; `rule-dark`): the 1px rule between timetable rows, menu separators, pressed button fill.
- **Ink** (`text`; `text-dark`): all primary text, and the fill of a checked checkbox.
- **Slate** (`muted`; `muted-dark`): secondary text, column headers, row actions at rest, hints, and placeholder text.
- **Board White / Board Slate** (`board-text`, `board-muted`): primary and secondary text on navy; identical in both modes.

### Named Rules
**The Amber Departure Rule.** Amber appears only on the next departure's countdown and on "Due now". With no upcoming reminder, the board countdown slot drops to `board-muted` at weight 300 (`boardCountdownIdle`). Ordinary countdowns in the table stay ink.

**The Two Ambers Rule.** Use `accent` on navy and `due-text` on the ground or table. Never put `accent` on a light surface; it fails contrast there.

**The One Board Rule.** Each surface carries at most one navy board mass: the header band, the overlay panel, or the popup card. Navy is never used for table rows or ordinary cards.

## Typography

**Display Font:** Segoe UI Variable Text (with Segoe UI)
**Body Font:** Segoe UI Variable Text (with Segoe UI)

**Character:** One native Windows family at every level. The countdown is set large and semibold so it reads across a room. Everything else is quiet.

Sizes are Qt points as written in `theme.cpp` (the app font is set to 10pt in `Theme::apply`). The overlay alone sets pixel sizes in code, because its columns are measured with `QFontMetrics` against the exact font.

### Hierarchy
- **Display** (600, 40pt): the board countdown (`boardCountdown`). Idle variant at 300 in Board Slate.
- **Headline** (600, 22pt): the popup's reminder title (`popupTitle`). **Headline Due** (600, 20pt): the popup's "Due now" / "Due N min ago" line (`popupDue`).
- **Title** (600, 19pt): the next reminder's title on the board (`boardTitle`), word-wrapped.
- **Cell Title** (600, 11pt): reminder titles in the timetable (`cellTitle`) and the empty-state title. Countdowns use the same size and weight (`cellCountdown`); "Due now" goes to 700 (`cellDue`).
- **Cell Figure** (400, 11pt): the departure time in the Time column (`cellTime`).
- **Body** (400, 10pt): app default; secondary cells such as day, repeats and subtask progress (`cellMuted`, in Slate).
- **Hint** (400, 9pt): the live "Understood as..." preview and examples under the input (`hint`), errors (`error`), and the board's greeting/clock line and next-reminder meta (`boardMuted`).
- **Label** (700, 8pt, 108% letter spacing, uppercase): timetable column headers (`colHeader`) and the "Recently done" section heading (`sectionTitle`). Built only through `makeCapsLabel()`, which uppercases the text and sets the spacing.
- **Overlay figures** (700, 21px next / 15px others; titles 15px DemiBold next / 13px regular others).

### Named Rules
**The Loudest Number Rule.** The next countdown is always the largest type on its surface. Nothing else on the board exceeds 19pt.

**The Column Header Rule.** Uppercase tracked labels name table columns or head a section's own list. They never sit above a larger heading as an eyebrow.

## Layout

The main window (minimum 780 x 520) is a vertical stack with no outer margin: the full-width board band on top (margins 24 left, 14 top, 20 right, 22 bottom; 14 between the header bar and the next-reminder row), then a body with 20px side margins and 16px top/bottom.

The timetable is a fixed column grid shared by the header row and every departure row: Time 112px, Reminder flexible, Repeats 168px, Due in 104px (right-aligned), actions 232px, with 12px gaps and 16px left row padding. Rows have 12px vertical padding. The column header row is offset to stay aligned with the list's 1px frame and its always-on vertical scrollbar (10px).

"Recently done" sits under the table, collapsed by default, showing up to two rows when opened. The input dock is last: a full-width line edit and the Add button with 8px between, then the hint line.

The overlay defaults to 280 x 380 and stacks rows with 14px side padding; the next row gets 10px vertical padding, the others 8px. Title and countdown columns are sized from measured text (title column at 60% of the width, countdown 72 to 132px).

The popup is 560 x 330: a board card (28px sides, 22 top, 24 bottom) over a ground body (28px sides, 18 top, 22 bottom) with Snooze and Done right-aligned, 10px apart.

Spacing steps in use: 2, 4, 8, 12, 16, 20, 24, 28 px.

## Elevation & Depth

Flat. The style sheet defines no shadows and no gradients. Depth is tonal: the navy board against the cool ground, the white table against the ground, `surface-alt` on hover and `rule` on press. The overlay is a frameless, translucent top-level window (`QWidget#overlayWindow` is transparent) whose only visible body is the rounded navy panel with a 1px Raised Board Navy border.

### Named Rules
**The Flat Board Rule.** Separate layers with tone and 1px lines, never with drop shadows.

## Shapes

Gently rounded, small radii, scaled to the element: checkbox 3px, menu items and the scrollbar handle 4px, inputs and every button 6px, the departure table and history list 8px, the overlay panel 10px. The board band and the popup board card are square and run edge to edge. Borders are always 1px. Transparent 1px borders are kept on borderless buttons, so focus and hover never shift the layout.

## Components

### Buttons
Plain, quiet, and labelled in words.
- **Shape:** gently rounded (6px), 1px border.
- **Default:** Paper fill, Ink text, Edge border, 7px 14px. Hover goes to Paper Hover; pressed goes to Hairline; disabled uses Slate text on Ground.
- **Primary** (`QPushButton#primaryButton`): Primary Navy fill and border, white text, weight 600, 8px 20px. One per surface: Add in the main window, Done in the popup.
- **Row actions** (`role="rowAction"`, `role="rowDanger"`): transparent, Slate text, 5px 10px. On hover they gain a Paper Hover fill, an Edge border and Ink text. Delete turns Signal Red on hover and focus.
- **Board buttons** (any `QPushButton` / `QToolButton` inside `QFrame#board`): transparent with white text, 6px 12px. Hover and checked use Raised Board Navy (checked adds a Board Slate border; the Overlay toggle is checkable). Focus is a 1px amber border. The menu indicator is hidden.
- **Focus:** 1px Focus Blue border everywhere except on the board.
- **Popup buttons:** 12pt, 10px 18px.

### Inputs / Fields
- **Style:** `QLineEdit`, `QPlainTextEdit`, `QTextEdit`, `QComboBox` and `QDateTimeEdit` share Paper fill, 1px Edge border, 6px radius, 7px 10px padding.
- **Focus:** border switches to Focus Blue. Selection is Focus Blue with white text.
- **Placeholder:** Slate, set twice on purpose: QSS `placeholder-text-color: @muted` and `QPalette::PlaceholderText` in `Theme::apply`. Keep both when extending; the palette entry covers widgets the style sheet does not reach.
- **Live preview:** the hint line under the main input reads "Understood as **title** · when · repeats", with the parsed title in bold. Errors replace it in Signal Red at the same size.

### Checkboxes
- **Style:** 14px indicator, 1px Slate border, 3px radius, Paper fill; hover border Focus Blue.
- **Checked:** Ink fill with a tick image: `:/theme/check-white.png` in light mode (white tick on navy) and `:/theme/check-ink.png` in dark mode (ink tick on the light Ink fill). Subtask checkboxes (`role="subtask"`) use Slate text.

### Menus and tooltips
- Paper fill, Edge border, 4px panel padding. Items 6px 22px 6px 14px with 4px radius and a Paper Hover selection. Separators are 1px Hairline with 4px 6px margins.

### Departures Board header (signature)
The board band carries a small Board Slate announcement line and live clock (separated by a dot) on the left, with Overlay, History and More on the right. The announcement reads "Now boarding: <title>" within 15 minutes of the next reminder and "Final call: <title>" once it's due; otherwise it shows the rotating greeting. Below that sit the next reminder's title (Title) and its when/repeat meta (Hint, Board Slate) on the left.

The countdown sits bottom-right as a **split-flap readout** (`FlapDisplay`, `src/flap_display.cpp`):
- Each character is its own Raised Board Navy tile (4px radius, 4px gap) with a Board Navy seam across the middle. Glyphs are Departure Amber, Segoe UI Variable Display 46px semibold. Colons and spaces are narrow, untiled separators.
- When a character changes, its tile flips: the top half folds down to the hinge, then the new bottom half unfolds, over 260ms OutCubic, with the moving flap shaded toward the seam colour.
- A change in length (59:59 to 1:00:00) snaps instead of flipping.
- Due state reads "DUE NOW". The idle state reads "--:--" in Board Slate.
- The flip is skipped when Windows "Show animations" is off (`SPI_GETCLIENTAREAANIMATION`) or `REMINDME_REDUCE_MOTION=1`.

### Departure table
Paper list with an 8px radius and an Edge outline, no selection highlight, no hover fill. Each row is divided by a 1px Hairline and shows: time plus day, the reminder title (with "Subtasks n/m done" and subtask checkboxes beneath it on repeating reminders), the repeat rule, the right-aligned countdown, and the row actions. When a row's countdown reaches zero, its Due-in cell switches role from `cellCountdown` to `cellDue` (amber, 700).

Under each row runs a 3px **journey line**: a Hairline track that fills as the reminder approaches, on one shared 6-hour scale (empty 6+ hours out, full when due). This keeps rows comparable. The next row fills in Due Amber (`due-text`); the others fill in Slate. Changing a `role` at runtime requires `unpolish` / `polish`; use the existing `setRole()` helper.

### Overlay panel
A single mini board: one navy panel (`role="overlayPanel"`, 10px radius) with rows separated by 1px Raised Board Navy rules (`overlayRule`). The next row is larger, its title semibold and its countdown in Departure Amber (`overlayTimeNext`). Other rows use white figures (`overlayTime`). Checklist progress and the "+N more" note are 8pt Board Slate (`overlayMeta`). It shows up to 6 rows.

### Due popup
The one loud moment. A navy board card holds the reminder title (Headline) and "Due today at ..." (Board Slate, 11pt). The ground body below leads with the due state in Due Amber, then "It's h:mm" in Slate 10pt, then Snooze 5 min (default button) and Done (primary).

## Do's and Don'ts

### Do:
- **Do** add every new colour as a `ThemePalette` field with a light and a dark value, plus a `@token` entry in `Theme::styleSheet()`. List longer token names before their prefixes (`@surfaceAlt` before `@surface`).
- **Do** style new widgets through a `role` property or an `objectName` selector in the app style sheet, not through per-widget `setStyleSheet` colours.
- **Do** keep Departure Amber for the next countdown and "Due now" only (The Amber Departure Rule).
- **Do** build uppercase labels with `makeCapsLabel()` (8pt, 700, 108% spacing), and only for column headers or a section's own heading.
- **Do** keep the timetable's fixed column widths shared between the header row and the data rows, so the columns line up.
- **Do** use 1px borders and hairline rules for separation, with 6px corners on controls.
- **Do** keep transparent 1px borders on borderless buttons so focus and hover do not shift the layout.

### Don't:
- **Don't** put `accent` amber on a light surface; use `due-text` there (The Two Ambers Rule).
- **Don't** add drop shadows or gradients; the system is flat (The Flat Board Rule).
- **Don't** add a second navy mass to a surface that already has a board, or use navy for rows and cards.
- **Don't** set a type size on a surface larger than its countdown (The Loudest Number Rule).
- **Don't** put an uppercase tracked kicker or eyebrow above a title. The unused `boardLabel` role in `theme.cpp` is not part of the system.
- **Don't** use a second filled primary button on one surface.
- **Don't** introduce another font family; Segoe UI Variable Text with Segoe UI fallback is the only face.

### Known gaps (recorded, not rules)
- Countdown figures do not set tabular figures explicitly; the overlay compensates by measuring columns with `QFontMetrics`.
- The Windows title bar (DWM caption) is not coloured to match the theme.
- A row reaching zero swaps its Due-in cell to amber with no motion. The system's motion is the board's split-flap flip (260ms) and the "Recently done" slide (180ms, OutCubic, on `maximumHeight`).
- In light mode, the `QComboBox` drop-down button shows a platform-default edge (visible in the edit dialog).
