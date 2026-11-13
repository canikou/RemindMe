// SPDX-License-Identifier: MIT
//! The app: main window (board + departures), always-on-top overlay, due popups, dialogs.
//! Runs as an iced daemon so timers keep firing with every window closed (tray mode).

use crate::flap::Flap;
use crate::platform::{self, External};
use crate::theme::{self as th, ButtonKind, Palette, pt};
use crate::updater::{self, CheckResult};
use chrono::{DateTime, Local, Timelike};
use iced::widget::{
    Column, button, checkbox, column, container, mouse_area, opaque, rich_text, row, scrollable,
    space, span, stack, text, text_editor, text_input,
};
use iced::{
    Alignment, Color, Element, Font, Length, Padding, Size, Subscription, Task, event, keyboard,
    time, window,
};
use remindme::edit::{self, EditForm};
use remindme::greetings::{self, Pools};
use remindme::model::{ChecklistItem, Reminder, ScheduleType, new_id};
use remindme::parser;
use remindme::schedule::{self as sched, SHORT_REPEAT_WARNING_SECONDS};
use remindme::store::{ReminderStore, documents_app_dir};
use remindme::time_format::{self as tf, hm};
use remindme::update::UpdateAsset;
use std::path::PathBuf;
use std::time::{Duration, Instant};

const MAIN_SIZE: Size = Size::new(1020.0, 700.0);
const MAIN_MIN: Size = Size::new(780.0, 520.0);
const POPUP_SIZE: Size = Size::new(560.0, 330.0);
const OVERLAY_WIDTH: f32 = 280.0;
const OVERLAY_MAX_ROWS: usize = 6;
const COMPLETED_PREVIEW_COUNT: usize = 2;
const COL_TIME: f32 = 112.0;
const COL_REPEATS: f32 = 168.0;
const COL_DUE: f32 = 104.0;
const COL_ACTIONS: f32 = 232.0;
const INPUT_ID: &str = "reminder-input";
/// The popup grabs focus when it appears; input aimed at the app the user was in is ignored this long.
const POPUP_INPUT_GUARD: Duration = Duration::from_millis(1500);

pub struct App {
    store: ReminderStore,
    palette: Palette,
    greetings: Pools,
    greeting: String,
    greeting_period: Option<usize>,
    now: DateTime<Local>,
    main: Option<window::Id>,
    overlay: Option<window::Id>,
    overlay_height: f32,
    popup: Option<Popup>,
    queue: Vec<String>,
    input: String,
    input_error: Option<String>,
    flap: Flap,
    completed_collapsed: bool,
    more_open: bool,
    modal: Option<Modal>,
    has_tray: bool,
    save_error_shown: bool,
    update_busy: bool,
}

struct Popup {
    window: window::Id,
    opened: Instant,
    reminder_id: String,
    title: String,
    due: DateTime<Local>,
}

struct SubtaskForm {
    id: String,
    title: String,
    items: Vec<ChecklistItem>,
    new_text: String,
}

enum Modal {
    Message {
        title: String,
        body: String,
    },
    ConfirmDelete {
        id: String,
        title: String,
    },
    ConfirmShortRepeat {
        reminder: Reminder,
        source: &'static str,
    },
    Edit(EditForm, Option<String>),
    Subtasks(SubtaskForm),
    History,
    Export(String),
    Import(text_editor::Content),
    UpdateAvailable {
        tag: String,
        release_url: String,
        asset: Option<UpdateAsset>,
    },
    InstallNow(PathBuf),
}

#[derive(Clone, Debug)]
pub enum Message {
    Tick,
    Frame,
    ThemeChanged(iced::theme::Mode),
    CloseRequested(window::Id),
    Escape(window::Id),
    Enter(window::Id),
    Input(String),
    Add,
    ToggleOverlay,
    DragOverlay,
    ToggleMore,
    CloseMore,
    OpenHistory,
    ToggleCompleted,
    ReAdd(String),
    Export,
    Import,
    ImportEdit(text_editor::Action),
    ImportConfirm,
    CloseModal,
    External(External),
    Edit(String),
    EditTitle(String),
    EditRelative(bool),
    EditNextIn(String),
    EditAt(String),
    EditRepeating(bool),
    EditRepeatEvery(String),
    EditSave,
    Delete(String),
    DeleteConfirmed,
    ShortRepeatConfirmed,
    Subtasks(String),
    SubtaskNewText(String),
    SubtaskAdd,
    SubtaskRemove(usize),
    SubtaskCheck(usize, bool),
    SubtasksSave,
    RowSubtaskToggled(String, usize, bool),
    PopupDone,
    PopupSnooze,
    StartupUpdateCheck,
    CheckUpdates(bool),
    UpdateChecked(bool, Result<CheckResult, String>),
    UpdateDownload,
    UpdateDownloaded(bool, Result<PathBuf, String>),
    InstallConfirmed,
    OpenUrl(String),
}

async fn blocking<T: Send + 'static>(f: impl FnOnce() -> T + Send + 'static) -> T {
    tokio::task::spawn_blocking(f)
        .await
        .expect("background task")
}

impl App {
    pub fn boot(has_tray: bool) -> (Self, Task<Message>) {
        let mut store = ReminderStore::new();
        let load_error = store.load().err();
        let greetings = documents_app_dir()
            .map(|d| greetings::load(&d))
            .unwrap_or_else(greetings::defaults);
        let mut app = Self {
            store,
            palette: if th::forced_dark() == Some(true) {
                th::DARK
            } else {
                th::LIGHT
            },
            greetings,
            greeting: String::new(),
            greeting_period: None,
            now: Local::now(),
            main: None,
            overlay: None,
            overlay_height: 0.0,
            popup: None,
            queue: Vec::new(),
            input: String::new(),
            input_error: None,
            flap: Flap::default(),
            completed_collapsed: true,
            more_open: false,
            modal: load_error.map(|body| Modal::Message {
                title: "Couldn't load reminders".into(),
                body,
            }),
            has_tray,
            save_error_shown: false,
            update_busy: false,
        };
        app.refresh_greeting();
        app.refresh_flap();
        let startup = Task::perform(tokio::time::sleep(Duration::from_millis(2500)), |_| {
            Message::StartupUpdateCheck
        });
        let theme = iced::system::theme().map(Message::ThemeChanged);
        let open = app.open_main();
        (app, Task::batch([open, theme, startup]))
    }

    // ---------- windows ----------

    fn open_main(&mut self) -> Task<Message> {
        if let Some(id) = self.main {
            return Task::batch([window::minimize(id, false), window::gain_focus(id)]);
        }
        let (id, open) = window::open(window::Settings {
            size: MAIN_SIZE,
            min_size: Some(MAIN_MIN),
            icon: platform::window_icon(),
            exit_on_close_request: false,
            platform_specific: platform_settings(),
            ..Default::default()
        });
        self.main = Some(id);
        Task::batch([open.discard(), window::gain_focus(id), text_input_focus()])
    }

    fn set_overlay(&mut self, visible: bool) -> Task<Message> {
        match (visible, self.overlay) {
            (true, None) => {
                self.overlay_height = self.overlay_content_height();
                let (id, open) = window::open(window::Settings {
                    size: Size::new(OVERLAY_WIDTH, self.overlay_height),
                    position: window::Position::SpecificWith(|win, monitor| {
                        iced::Point::new(monitor.width - win.width - 20.0, 20.0)
                    }),
                    resizable: false,
                    decorations: false,
                    level: window::Level::AlwaysOnTop,
                    icon: platform::window_icon(),
                    exit_on_close_request: false,
                    platform_specific: overlay_platform_settings(),
                    ..Default::default()
                });
                self.overlay = Some(id);
                keep_on_top(open)
            }
            (false, Some(id)) => {
                self.overlay = None;
                window::close(id)
            }
            _ => Task::none(),
        }
    }

    fn quit(&mut self) -> Task<Message> {
        self.save();
        iced::exit()
    }

    // ---------- state helpers ----------

    fn refresh_greeting(&mut self) {
        let period = greetings::period_for_hour(self.now.hour());
        if self.greeting_period == Some(period) && !self.greeting.is_empty() {
            return;
        }
        self.greeting_period = Some(period);
        let pool = &self.greetings[period];
        if !pool.is_empty() {
            self.greeting = pool[rand::random_range(0..pool.len())].clone();
        }
    }

    fn refresh_flap(&mut self) {
        let text = match self.store.items.first() {
            None => "--:--".to_string(),
            Some(next) => {
                let left = (next.next - self.now).num_seconds();
                if left <= 0 {
                    "DUE NOW".into()
                } else {
                    tf::format_countdown(left)
                }
            }
        };
        self.flap.set_text(&text, Instant::now());
    }

    fn save(&mut self) {
        match self.store.save() {
            Ok(()) => self.save_error_shown = false,
            Err(body) if !self.save_error_shown => {
                self.save_error_shown = true;
                self.modal = Some(Modal::Message {
                    title: "Save error".into(),
                    body,
                });
            }
            Err(_) => {}
        }
    }

    /// Sort, save, and keep the overlay sized to its rows.
    fn commit(&mut self) -> Task<Message> {
        self.store.sort_soonest_first();
        self.save();
        self.refresh_flap();
        self.resize_overlay()
    }

    fn resize_overlay(&mut self) -> Task<Message> {
        let Some(id) = self.overlay else {
            return Task::none();
        };
        let h = self.overlay_content_height();
        if (h - self.overlay_height).abs() < 0.5 {
            return Task::none();
        }
        self.overlay_height = h;
        window::resize(id, Size::new(OVERLAY_WIDTH, h))
    }

    fn find(&self, id: &str) -> Option<&Reminder> {
        self.store.items.iter().find(|r| r.id == id)
    }

    fn find_mut(&mut self, id: &str) -> Option<&mut Reminder> {
        self.store.items.iter_mut().find(|r| r.id == id)
    }

    /// Shows a dialog, opening the main window first if it's in the tray.
    fn show_modal(&mut self, modal: Modal) -> Task<Message> {
        self.modal = Some(modal);
        self.more_open = false;
        self.open_main()
    }

    fn message(&mut self, title: &str, body: impl Into<String>) -> Task<Message> {
        self.show_modal(Modal::Message {
            title: title.into(),
            body: body.into(),
        })
    }

    // ---------- due popups ----------

    fn trigger_due(&mut self) {
        for r in &self.store.items {
            let active = self.popup.as_ref().is_some_and(|p| p.reminder_id == r.id);
            if r.next <= self.now && !active && !self.queue.contains(&r.id) {
                self.queue.push(r.id.clone());
            }
        }
    }

    fn show_next_popup(&mut self) -> Task<Message> {
        if self.popup.is_some() {
            return Task::none();
        }
        while !self.queue.is_empty() {
            let id = self.queue.remove(0);
            let Some(r) = self.find(&id).filter(|r| r.next <= self.now) else {
                continue;
            };
            let (title, due) = (r.title.clone(), r.next);
            let (window, open) = window::open(window::Settings {
                size: POPUP_SIZE,
                position: window::Position::Centered,
                level: window::Level::AlwaysOnTop,
                icon: platform::window_icon(),
                exit_on_close_request: false,
                platform_specific: platform_settings(),
                ..Default::default()
            });
            self.popup = Some(Popup {
                window,
                opened: Instant::now(),
                reminder_id: id,
                title,
                due,
            });
            return Task::batch([
                keep_on_top(open),
                window::gain_focus(window),
                window::request_user_attention(window, Some(window::UserAttention::Critical)),
            ]);
        }
        Task::none()
    }

    fn finish_popup(&mut self, done: bool) -> Task<Message> {
        let Some(p) = self.popup.take() else {
            return Task::none();
        };
        edit::finish_reminder(&mut self.store, &p.reminder_id, done, Local::now());
        self.queue.retain(|q| *q != p.reminder_id);
        Task::batch([window::close(p.window), self.commit()])
    }

    // ---------- update ----------

    pub fn update(&mut self, message: Message) -> Task<Message> {
        if std::env::var_os("REMINDME_DEBUG").is_some()
            && !matches!(message, Message::Tick | Message::Frame)
        {
            eprintln!("[debug] {message:?}");
        }
        match message {
            Message::Tick => {
                self.now = Local::now();
                self.refresh_greeting();
                self.trigger_due();
                self.refresh_flap();
                return Task::batch([self.show_next_popup(), self.resize_overlay()]);
            }
            Message::External(e) => {
                return match e {
                    External::ShowMain => self.open_main(),
                    External::CheckUpdates => self.update(Message::CheckUpdates(true)),
                    External::Quit => self.quit(),
                };
            }
            Message::Frame => {}
            // None means "unknown" (e.g. a new X11 window); keep what we have.
            Message::ThemeChanged(mode) if mode != iced::theme::Mode::None => {
                let dark = th::forced_dark().unwrap_or(mode == iced::theme::Mode::Dark);
                self.palette = if dark { th::DARK } else { th::LIGHT };
            }
            Message::ThemeChanged(_) => {}
            Message::CloseRequested(id) => {
                if self.popup.as_ref().is_some_and(|p| p.window == id) {
                    // Closing with X counts as done, but not while the popup is still stealing focus.
                    if !self.popup_takes_keys(id) {
                        return Task::none();
                    }
                    return self.finish_popup(true);
                }
                if Some(id) == self.overlay {
                    return self.set_overlay(false);
                }
                if Some(id) == self.main {
                    if !self.has_tray {
                        return self.quit();
                    }
                    // Lives on in the tray: timers keep running with the window closed.
                    self.save();
                    self.main = None;
                    self.modal = None;
                    self.more_open = false;
                }
                return window::close(id);
            }
            Message::Escape(id) => {
                if self.popup_takes_keys(id) {
                    return self.finish_popup(false); // Esc snoozes, so nothing is left stuck
                }
                if Some(id) == self.main {
                    self.more_open = false;
                    if !matches!(self.modal, Some(Modal::ConfirmShortRepeat { .. })) {
                        self.modal = None;
                    }
                }
            }
            Message::Enter(id) => {
                if self.popup_takes_keys(id) {
                    return self.finish_popup(true);
                }
            }
            Message::PopupDone => return self.finish_popup(true),
            Message::PopupSnooze => return self.finish_popup(false),
            Message::Input(v) => {
                self.input = v;
                self.input_error = None;
            }
            Message::Add => return self.add_from_input(),
            Message::ToggleOverlay => return self.set_overlay(self.overlay.is_none()),
            Message::DragOverlay => {
                if let Some(id) = self.overlay {
                    return window::drag(id);
                }
            }
            Message::ToggleMore => self.more_open = !self.more_open,
            Message::CloseMore => self.more_open = false,
            Message::OpenHistory => return self.show_modal(Modal::History),
            Message::ToggleCompleted => self.completed_collapsed = !self.completed_collapsed,
            Message::ReAdd(cid) => {
                if let Some(c) = self.store.completed.iter().find(|c| c.id == cid) {
                    let r = sched::reminder_from_completed(c, Local::now());
                    self.store.items.push(r);
                    return self.commit();
                }
            }
            Message::Export => {
                let share = self.store.export_share_string();
                let copy = iced::clipboard::write(share.clone());
                return Task::batch([copy, self.show_modal(Modal::Export(share))]);
            }
            Message::Import => return self.show_modal(Modal::Import(text_editor::Content::new())),
            Message::ImportEdit(action) => {
                if let Some(Modal::Import(content)) = &mut self.modal {
                    content.perform(action);
                }
            }
            Message::ImportConfirm => {
                let Some(Modal::Import(content)) = &self.modal else {
                    return Task::none();
                };
                match self.store.import_share_string(&content.text()) {
                    Ok(n) => {
                        let commit = self.commit();
                        return Task::batch([
                            commit,
                            self.message("Import complete", format!("Imported {n} reminder(s).")),
                        ]);
                    }
                    Err(e) => return self.message("Import error", e),
                }
            }
            Message::CloseModal => {
                // Declined installer: don't leave the download behind.
                if let Some(Modal::InstallNow(path)) = self.modal.take() {
                    let _ = std::fs::remove_file(&path);
                }
            }
            Message::Edit(id) => {
                if let Some(r) = self.find(&id) {
                    let form = edit::edit_form(r, Local::now());
                    return self.show_modal(Modal::Edit(form, None));
                }
            }
            Message::EditTitle(v) => self.with_edit(|f| f.title = v),
            Message::EditRelative(v) => self.with_edit(|f| f.relative = v),
            Message::EditNextIn(v) => self.with_edit(|f| f.next_in = v),
            Message::EditAt(v) => self.with_edit(|f| f.at = v),
            Message::EditRepeating(v) => self.with_edit(|f| f.repeating = v),
            Message::EditRepeatEvery(v) => self.with_edit(|f| f.repeat_every = v),
            Message::EditSave => return self.save_edit(),
            Message::Delete(id) => {
                if let Some(r) = self.find(&id) {
                    let title = r.title.clone();
                    return self.show_modal(Modal::ConfirmDelete { id, title });
                }
            }
            Message::DeleteConfirmed => {
                if let Some(Modal::ConfirmDelete { id, .. }) = self.modal.take() {
                    self.store.items.retain(|r| r.id != id);
                    self.queue.retain(|q| *q != id);
                    let mut tasks = vec![self.commit()];
                    if let Some(p) = self.popup.take_if(|p| p.reminder_id == id) {
                        tasks.push(window::close(p.window));
                    }
                    return Task::batch(tasks);
                }
            }
            Message::ShortRepeatConfirmed => {
                if let Some(Modal::ConfirmShortRepeat { reminder, source }) = self.modal.take() {
                    if source == "adding" {
                        self.input.clear();
                    }
                    let close = self.upsert(reminder);
                    return Task::batch([close, self.commit()]);
                }
            }
            Message::Subtasks(id) => {
                if let Some(r) = self.find(&id).filter(|r| r.repeating) {
                    let form = SubtaskForm {
                        id,
                        title: r.title.clone(),
                        items: r.checklist.clone(),
                        new_text: String::new(),
                    };
                    return self.show_modal(Modal::Subtasks(form));
                }
            }
            Message::SubtaskNewText(v) => self.with_subtasks(|f| f.new_text = v),
            Message::SubtaskAdd => self.with_subtasks(|f| {
                let t = f.new_text.trim().to_string();
                if !t.is_empty() {
                    f.items.push(ChecklistItem {
                        text: t,
                        checked: false,
                    });
                    f.new_text.clear();
                }
            }),
            Message::SubtaskRemove(i) => self.with_subtasks(|f| {
                if i < f.items.len() {
                    f.items.remove(i);
                }
            }),
            Message::SubtaskCheck(i, v) => self.with_subtasks(|f| {
                if let Some(item) = f.items.get_mut(i) {
                    item.checked = v;
                }
            }),
            Message::SubtasksSave => {
                if let Some(Modal::Subtasks(form)) = self.modal.take()
                    && let Some(r) = self.find_mut(&form.id)
                {
                    r.checklist = form
                        .items
                        .into_iter()
                        .filter(|i| !i.text.trim().is_empty())
                        .collect();
                    r.enforce_checklist_constraints();
                    return self.commit();
                }
            }
            Message::RowSubtaskToggled(id, i, v) => {
                if let Some(item) = self.find_mut(&id).and_then(|r| r.checklist.get_mut(i))
                    && item.checked != v
                {
                    item.checked = v;
                    return self.commit();
                }
            }
            Message::StartupUpdateCheck => {
                if updater::should_check_now() {
                    return self.update(Message::CheckUpdates(false));
                }
            }
            Message::CheckUpdates(user) => {
                self.more_open = false;
                if self.update_busy {
                    return if user {
                        self.message("Update check", "An update check is already running.")
                    } else {
                        Task::none()
                    };
                }
                self.update_busy = true;
                return Task::perform(blocking(updater::check), move |r| {
                    Message::UpdateChecked(user, r)
                });
            }
            Message::UpdateChecked(user, result) => {
                self.update_busy = false;
                return match result {
                    Err(e) if user => self.message("Update check", e),
                    Ok(CheckResult::UpToDate) if user => {
                        self.message("Update check", "You're already on the latest version.")
                    }
                    Ok(CheckResult::Available {
                        tag,
                        release_url,
                        asset,
                    }) => {
                        // Outside an AppImage (e.g. a distro package) Linux can't self-update; offer the page.
                        let asset =
                            asset.filter(|_| cfg!(windows) || updater::appimage_path().is_some());
                        self.show_modal(Modal::UpdateAvailable {
                            tag,
                            release_url,
                            asset,
                        })
                    }
                    _ => Task::none(),
                };
            }
            Message::UpdateDownload => {
                if let Some(Modal::UpdateAvailable {
                    asset: Some(asset), ..
                }) = self.modal.take()
                {
                    self.update_busy = true;
                    let installer = asset.is_installer;
                    return Task::perform(blocking(move || updater::download(&asset)), move |r| {
                        Message::UpdateDownloaded(installer, r)
                    });
                }
            }
            Message::UpdateDownloaded(installer, result) => {
                self.update_busy = false;
                return match result {
                    Err(e) => self.message("Update download", e),
                    Ok(path) if installer => self.show_modal(Modal::InstallNow(path)),
                    Ok(path) => {
                        if let Some(dir) = path.parent() {
                            platform::open_url(&dir.to_string_lossy());
                        }
                        self.message(
                            "Update downloaded",
                            format!("Downloaded the latest release package to:\n{}\n\nNo installer was detected in this asset. You can install it manually.", path.display()),
                        )
                    }
                };
            }
            Message::InstallConfirmed => {
                if let Some(Modal::InstallNow(path)) = self.modal.take() {
                    return match updater::install(&path) {
                        Ok(()) => self.quit(),
                        Err(e) => self.message("Install update", e),
                    };
                }
            }
            Message::OpenUrl(url) => {
                self.modal = None;
                platform::open_url(&url);
            }
        }
        Task::none()
    }

    /// The popup grabs focus when it appears; ignore keys meant for the app the user was typing in.
    fn popup_takes_keys(&self, id: window::Id) -> bool {
        self.popup
            .as_ref()
            .is_some_and(|p| p.window == id && p.opened.elapsed() >= POPUP_INPUT_GUARD)
    }

    fn with_edit(&mut self, f: impl FnOnce(&mut EditForm)) {
        if let Some(Modal::Edit(form, error)) = &mut self.modal {
            f(form);
            *error = None;
        }
    }

    fn with_subtasks(&mut self, f: impl FnOnce(&mut SubtaskForm)) {
        if let Some(Modal::Subtasks(form)) = &mut self.modal {
            f(form);
        }
    }

    /// Closes the reminder's popup, if shown: it describes the old schedule and Done/Snooze would act on the edit.
    fn upsert(&mut self, reminder: Reminder) -> Task<Message> {
        self.queue.retain(|q| *q != reminder.id);
        let stale = self.popup.take_if(|p| p.reminder_id == reminder.id);
        match self.store.items.iter_mut().find(|r| r.id == reminder.id) {
            Some(existing) => *existing = reminder,
            None => self.store.items.push(reminder),
        }
        stale.map_or_else(Task::none, |p| window::close(p.window))
    }

    /// Adds or replaces, asking first when a repeat is so short it would flood popups.
    fn upsert_checked(&mut self, reminder: Reminder, source: &'static str) -> Task<Message> {
        let short = reminder.repeating
            && reminder.schedule_type == ScheduleType::Relative
            && reminder.interval_seconds > 0
            && reminder.interval_seconds < SHORT_REPEAT_WARNING_SECONDS;
        if short {
            return self.show_modal(Modal::ConfirmShortRepeat { reminder, source });
        }
        self.modal = None;
        if source == "adding" {
            self.input.clear();
        }
        let close = self.upsert(reminder);
        Task::batch([close, self.commit()])
    }

    fn add_from_input(&mut self) -> Task<Message> {
        if self.input.trim().is_empty() {
            return text_input_focus();
        }
        let now = Local::now();
        match parser::parse_input_at(&self.input, now) {
            // Inline instead of a dialog so the text can be fixed in place; typing clears it.
            Err(e) => {
                self.input_error = Some(e);
                text_input_focus()
            }
            Ok(parsed) => {
                let mut r = sched::reminder_from_parse(&parsed, now);
                r.id = new_id();
                self.upsert_checked(r, "adding")
            }
        }
    }

    fn save_edit(&mut self) -> Task<Message> {
        let Some(Modal::Edit(form, _)) = &self.modal else {
            return Task::none();
        };
        let Some(original) = self.find(&form.id) else {
            return self.message(
                "Reminder gone",
                "This reminder was completed or deleted while you were editing it.",
            );
        };
        let now = Local::now();
        match edit::edited_reminder(original, form, now) {
            // Validate before closing so a typo doesn't throw away the other edits.
            Err(e) => {
                if let Some(Modal::Edit(_, error)) = &mut self.modal {
                    *error = Some(e);
                }
                Task::none()
            }
            Ok(r) => self.upsert_checked(r, "editing"),
        }
    }

    // ---------- subscriptions ----------

    pub fn subscription(&self) -> Subscription<Message> {
        let mut subs = vec![
            time::every(Duration::from_secs(1)).map(|_| Message::Tick),
            Subscription::run(external_events),
            window::close_requests().map(Message::CloseRequested),
            iced::system::theme_changes().map(Message::ThemeChanged),
            event::listen_with(|e, _, id| match e {
                iced::Event::Keyboard(keyboard::Event::KeyPressed {
                    key: keyboard::Key::Named(named),
                    ..
                }) => match named {
                    keyboard::key::Named::Escape => Some(Message::Escape(id)),
                    keyboard::key::Named::Enter => Some(Message::Enter(id)),
                    _ => None,
                },
                _ => None,
            }),
        ];
        if self.flap.animating(Instant::now()) {
            subs.push(window::frames().map(|_| Message::Frame));
        }
        Subscription::batch(subs)
    }

    pub fn title(&self, id: window::Id) -> String {
        if let Some(p) = self.popup.as_ref().filter(|p| p.window == id) {
            let overdue = (self.now - p.due).num_seconds();
            let mut t = format!("{} | {}", p.title, tf::format_due_date_time(&p.due));
            if overdue >= 60 {
                t += &format!(" | {}", tf::format_overdue_text(overdue));
            }
            return t;
        }
        if Some(id) == self.overlay {
            "RemindMe Overlay".into()
        } else {
            remindme::APP_NAME.into()
        }
    }

    pub fn theme(&self, _: window::Id) -> iced::Theme {
        th::iced_theme(&self.palette)
    }

    pub fn style(&self, _: &iced::Theme) -> iced::theme::Style {
        iced::theme::Style {
            background_color: self.palette.ground,
            text_color: self.palette.text,
        }
    }

    // ---------- views ----------

    pub fn view(&self, id: window::Id) -> Element<'_, Message> {
        if Some(id) == self.overlay {
            return self.overlay_view();
        }
        if let Some(p) = self.popup.as_ref().filter(|p| p.window == id) {
            return self.popup_view(p);
        }
        self.main_view()
    }

    fn main_view(&self) -> Element<'_, Message> {
        let p = self.palette;
        let content = column![self.board_view(), self.body_view()];
        let mut layers = stack![content].width(Length::Fill).height(Length::Fill);
        if self.more_open {
            let item = |label: &'static str, m: Message| {
                button(text(label).size(pt(10.0)))
                    .width(Length::Fill)
                    .padding([6, 14])
                    .style(th::button_style(p, ButtonKind::RowAction))
                    .on_press(m)
            };
            let menu = container(
                column![
                    item("Export reminders…", Message::Export),
                    item("Import reminders…", Message::Import),
                    container(space().height(1))
                        .width(Length::Fill)
                        .style(th::panel(p.rule, None, 0.0)),
                    item("Check for updates", Message::CheckUpdates(true)),
                ]
                .spacing(2)
                .width(220),
            )
            .padding(4)
            .style(th::panel(p.surface, Some(p.border), 6.0));
            layers = layers
                .push(
                    mouse_area(space().width(Length::Fill).height(Length::Fill))
                        .on_press(Message::CloseMore),
                )
                .push(container(menu).align_right(Length::Fill).padding(Padding {
                    top: 50.0,
                    right: 20.0,
                    ..Padding::ZERO
                }));
        }
        if let Some(modal) = &self.modal {
            let dialog = container(self.modal_view(modal))
                .padding(20)
                .max_width(640)
                .style(th::panel(p.ground, Some(p.border), 10.0));
            let backdrop = container(dialog)
                .center(Length::Fill)
                .padding(24)
                .style(th::panel(
                    Color {
                        a: 0.55,
                        ..Color::BLACK
                    },
                    None,
                    0.0,
                ));
            layers = layers.push(opaque(backdrop));
        }
        layers.into()
    }

    fn board_view(&self) -> Element<'_, Message> {
        let p = self.palette;
        let muted = |s: String| text(s).size(pt(9.0)).color(p.board_muted);
        let next = self.store.items.first();
        let left = next.map_or(i64::MAX, |n| (n.next - self.now).num_seconds());

        // Station-style announcement when the next one is close; the greeting otherwise.
        let headline: Element<'_, Message> = match next {
            Some(n) if left <= sched::BOARDING_SECONDS => {
                let (label, color) = if left <= 0 {
                    ("FINAL CALL", p.accent)
                } else {
                    ("NOW BOARDING", p.board_text)
                };
                rich_text![
                    span::<(), Font>(label).font(th::BOLD).color(color),
                    span::<(), Font>(format!("  {}", n.title)).color(p.board_muted)
                ]
                .size(pt(9.0))
                .into()
            }
            _ => muted(self.greeting.clone()).into(),
        };
        let board_btn = |label: &'static str, m: Option<Message>, kind: ButtonKind| {
            button(text(label).size(pt(10.0)))
                .padding([6, 12])
                .style(th::button_style(p, kind))
                .on_press_maybe(m)
        };
        let bar = row![
            headline,
            muted("  ·  ".into()),
            muted(tf::format_clock_time(&self.now)),
            space::horizontal(),
            board_btn(
                "Overlay",
                Some(Message::ToggleOverlay),
                if self.overlay.is_some() {
                    ButtonKind::BoardChecked
                } else {
                    ButtonKind::Board
                }
            ),
            board_btn(
                "History",
                (!self.store.completed.is_empty()).then_some(Message::OpenHistory),
                ButtonKind::Board
            ),
            board_btn("More", Some(Message::ToggleMore), ButtonKind::Board),
        ]
        .spacing(4)
        .align_y(Alignment::Center);

        let (title, meta) = match next {
            None => (
                "Nothing scheduled".to_string(),
                "Add one below and it'll show up here.".to_string(),
            ),
            Some(n) => (
                n.title.clone(),
                format!(
                    "{}, {}  ·  {}",
                    sched::due_day_text(n.next, self.now),
                    hm(&n.next),
                    sched::repeats_cell_text(n)
                ),
            ),
        };
        let glyph = if next.is_some() {
            p.accent
        } else {
            p.board_muted
        };
        let next_row = row![
            column![
                text(title)
                    .size(pt(19.0))
                    .font(th::SEMIBOLD)
                    .color(p.board_text),
                muted(meta)
            ]
            .spacing(2)
            .width(Length::Fill),
            self.flap.view(46.0, p.board_raised, glyph, p.board),
        ]
        .spacing(24)
        .align_y(Alignment::End);

        container(column![bar, next_row].spacing(14))
            .padding(Padding {
                top: 14.0,
                right: 20.0,
                bottom: 22.0,
                left: 24.0,
            })
            .width(Length::Fill)
            .style(th::panel(p.board, None, 0.0))
            .into()
    }

    fn body_view(&self) -> Element<'_, Message> {
        let p = self.palette;
        let departures: Element<'_, Message> = if self.store.items.is_empty() {
            container(
                column![
                    text("No reminders yet").size(pt(11.0)).font(th::SEMIBOLD),
                    text("Type what and when in the box below, then press Enter.\nDrink water in 45m  ·  Stand up at 7:00AM every weekday  ·  Dentist on Oct 24 6:30AM")
                        .color(p.muted)
                        .align_x(Alignment::Center),
                ]
                .spacing(6)
                .align_x(Alignment::Center),
            )
            .center(Length::Fill)
            .into()
        } else {
            let caps = |s: &'static str| text(s).size(pt(8.0)).font(th::BOLD).color(p.muted);
            let header = row![
                caps("TIME").width(COL_TIME),
                caps("REMINDER").width(Length::Fill),
                caps("REPEATS").width(COL_REPEATS),
                caps("DUE IN").width(COL_DUE).align_x(Alignment::End),
                space().width(COL_ACTIONS),
            ]
            .spacing(12)
            .padding(Padding {
                left: 17.0,
                right: 19.0,
                ..Padding::ZERO
            });
            let rows = Column::with_children(
                self.store
                    .items
                    .iter()
                    .enumerate()
                    .map(|(i, r)| self.departure_row(r, i == 0)),
            );
            let list = container(
                scrollable(rows)
                    .style(th::scroll_style(p))
                    .height(Length::Fill),
            )
            .style(th::panel(p.surface, Some(p.border), 8.0))
            .padding(1);
            column![header, list].spacing(6).into()
        };

        let mut body = column![departures].spacing(10).height(Length::Fill);
        if !self.store.completed.is_empty() {
            body = body.push(self.completed_preview());
        }
        let input = text_input("What and when? e.g. Drink water in 45m", &self.input)
            .id(INPUT_ID)
            .on_input(Message::Input)
            .on_submit(Message::Add)
            .padding([7, 10])
            .style(th::input_style(p));
        let add = button(text("Add").font(th::SEMIBOLD))
            .padding([8, 20])
            .style(th::button_style(p, ButtonKind::Primary))
            .on_press(Message::Add);
        body = body
            .push(space().height(4))
            .push(row![input, add].spacing(8));

        let hint: Element<'_, Message> = match &self.input_error {
            Some(e) => text(e.clone()).size(pt(9.0)).color(p.danger).into(),
            None => rich_text(
                sched::input_preview(&self.input, self.now)
                    .into_iter()
                    .map(|(s, bold)| {
                        span::<(), Font>(s).font(if bold { th::BOLD } else { Font::DEFAULT })
                    })
                    .collect::<Vec<_>>(),
            )
            .size(pt(9.0))
            .color(p.muted)
            .into(),
        };
        container(body.push(hint))
            .padding([16, 20])
            .height(Length::Fill)
            .into()
    }

    fn departure_row<'a>(&'a self, r: &'a Reminder, is_next: bool) -> Element<'a, Message> {
        let p = self.palette;
        let left = (r.next - self.now).num_seconds();
        let time_col = column![
            text(hm(&r.next)).size(pt(11.0)),
            text(sched::due_day_text(r.next, self.now))
                .size(pt(10.0))
                .color(p.muted)
        ]
        .spacing(1)
        .width(COL_TIME);

        let mut info = column![text(&r.title).size(pt(11.0)).font(th::SEMIBOLD)]
            .spacing(4)
            .width(Length::Fill);
        if !r.checklist.is_empty() {
            info = info.push(
                text(sched::subtask_progress_text(r))
                    .size(pt(10.0))
                    .color(p.muted),
            );
            for (i, item) in r
                .checklist
                .iter()
                .enumerate()
                .filter(|(_, i)| !i.checked && !i.text.trim().is_empty())
            {
                let id = r.id.clone();
                info = info.push(
                    checkbox(false)
                        .label(item.text.trim())
                        .size(14)
                        .text_size(pt(10.0))
                        .style(th::checkbox_style(p, true))
                        .on_toggle(move |v| Message::RowSubtaskToggled(id.clone(), i, v)),
                );
            }
        }

        let action = |label: &'static str, kind: ButtonKind, m: Message| {
            button(text(label).size(pt(10.0)))
                .padding([5, 10])
                .style(th::button_style(p, kind))
                .on_press(m)
        };
        let mut actions = row![space::horizontal()].spacing(2).width(COL_ACTIONS);
        if r.repeating {
            actions = actions.push(action(
                "Subtasks",
                ButtonKind::RowAction,
                Message::Subtasks(r.id.clone()),
            ));
        }
        actions = actions
            .push(action(
                "Edit",
                ButtonKind::RowAction,
                Message::Edit(r.id.clone()),
            ))
            .push(action(
                "Delete",
                ButtonKind::RowDanger,
                Message::Delete(r.id.clone()),
            ));

        let due = left <= 0;
        let cells = row![
            time_col,
            info,
            text(sched::repeats_cell_text(r))
                .size(pt(10.0))
                .color(p.muted)
                .width(COL_REPEATS),
            text(sched::countdown_cell_text(left))
                .size(pt(11.0))
                .font(if due { th::BOLD } else { th::SEMIBOLD })
                .color(if due { p.due_text } else { p.text })
                .width(COL_DUE)
                .align_x(Alignment::End),
            actions,
        ]
        .spacing(12);

        // Thin track that fills as the reminder approaches; nothing at all 6+ hours out.
        let progress = sched::journey_progress(r, self.now);
        let journey: Element<'_, Message> = if progress <= 0.0 {
            space().height(3).into()
        } else {
            let filled = (progress * 1000.0).round() as u16;
            let bar = |c: Color, portion: u16| {
                container(space().height(3))
                    .width(Length::FillPortion(portion.max(1)))
                    .style(th::panel(c, None, 1.5))
            };
            if filled >= 1000 {
                bar(if is_next { p.due_text } else { p.muted }, 1)
                    .width(Length::Fill)
                    .into()
            } else {
                row![
                    bar(if is_next { p.due_text } else { p.muted }, filled),
                    bar(p.rule, 1000 - filled)
                ]
                .into()
            }
        };

        let rule = container(space().height(1))
            .width(Length::Fill)
            .style(th::panel(p.rule, None, 0.0));
        column![
            container(column![cells, journey].spacing(10)).padding(Padding {
                top: 12.0,
                right: 8.0,
                bottom: 10.0,
                left: 16.0
            }),
            rule
        ]
        .into()
    }

    fn completed_row<'a>(&self, c: &'a remindme::model::CompletedReminder) -> Element<'a, Message> {
        let p = self.palette;
        row![
            column![
                text(sched::completed_title_text(c))
                    .size(pt(11.0))
                    .font(th::SEMIBOLD),
                text(format!(
                    "Done {}  ·  {}",
                    c.completed_at.format("%b %-d, %-I:%M %p"),
                    sched::completed_pattern_text(c)
                ))
                .size(pt(10.0))
                .color(p.muted),
            ]
            .spacing(2)
            .width(Length::Fill),
            button(text("Add again").size(pt(10.0)))
                .padding([5, 10])
                .style(th::button_style(p, ButtonKind::RowAction))
                .on_press(Message::ReAdd(c.id.clone())),
        ]
        .spacing(12)
        .align_y(Alignment::Center)
        .padding(Padding {
            top: 8.0,
            right: 12.0,
            bottom: 8.0,
            left: 16.0,
        })
        .into()
    }

    fn completed_preview(&self) -> Element<'_, Message> {
        let p = self.palette;
        let toggle = button(
            text(if self.completed_collapsed {
                "Show ▸"
            } else {
                "Hide ▾"
            })
            .size(pt(9.0)),
        )
        .padding([2, 8])
        .style(th::button_style(p, ButtonKind::RowAction))
        .on_press(Message::ToggleCompleted);
        let mut section = column![
            row![
                text("RECENTLY DONE")
                    .size(pt(8.0))
                    .font(th::BOLD)
                    .color(p.muted),
                space::horizontal(),
                toggle
            ]
            .align_y(Alignment::Center)
        ]
        .spacing(4);
        if !self.completed_collapsed {
            let rows = Column::with_children(
                self.store
                    .completed
                    .iter()
                    .rev()
                    .take(COMPLETED_PREVIEW_COUNT)
                    .map(|c| self.completed_row(c)),
            );
            section = section.push(container(rows).width(Length::Fill).style(th::panel(
                p.surface,
                Some(p.border),
                8.0,
            )));
        }
        section.into()
    }

    // ---------- dialogs ----------

    fn modal_view<'a>(&'a self, modal: &'a Modal) -> Element<'a, Message> {
        let p = self.palette;
        let heading = |s: &str| text(s.to_string()).size(pt(13.0)).font(th::SEMIBOLD);
        let btn = |label: &'static str, kind: ButtonKind, m: Message| {
            button(text(label))
                .padding([7, 14])
                .style(th::button_style(p, kind))
                .on_press(m)
        };
        let buttons = |items: Vec<Element<'a, Message>>| {
            row(std::iter::once(space::horizontal().into()).chain(items)).spacing(8)
        };
        let field = |placeholder: &str, value: &str, on: fn(String) -> Message| {
            text_input(placeholder, value)
                .on_input(on)
                .padding([7, 10])
                .style(th::input_style(p))
        };
        let label = |s: &'static str| text(s).width(110).color(p.muted);

        match modal {
            Modal::Message { title, body } => column![heading(title), text(body.clone()), buttons(vec![btn("OK", ButtonKind::Primary, Message::CloseModal).into()])]
                .spacing(14)
                .into(),
            Modal::ConfirmDelete { title, .. } => column![
                heading("Delete reminder"),
                text(format!("Delete \"{title}\"? This can't be undone.")),
                buttons(vec![btn("Cancel", ButtonKind::Normal, Message::CloseModal).into(), btn("Delete", ButtonKind::Primary, Message::DeleteConfirmed).into()]),
            ]
            .spacing(14)
            .into(),
            Modal::ConfirmShortRepeat { reminder, source } => column![
                heading("Very short repeating interval"),
                text(format!(
                    "This repeating reminder interval is very short ({}).\nThat can trigger many popups very quickly.\n\nContinue {source} this reminder?",
                    tf::format_interval_text(reminder.interval_seconds as i64)
                )),
                buttons(vec![btn("No", ButtonKind::Normal, Message::CloseModal).into(), btn("Yes", ButtonKind::Primary, Message::ShortRepeatConfirmed).into()]),
            ]
            .spacing(14)
            .into(),
            Modal::Edit(f, error) => {
                let kind = |l: &'static str, on: bool, m: Message| {
                    button(text(l)).padding([6, 12]).style(th::button_style(p, if on { ButtonKind::Primary } else { ButtonKind::Normal })).on_press(m)
                };
                let mut form = column![
                    heading("Edit reminder"),
                    row![label("Title"), field("Title", &f.title, Message::EditTitle)].align_y(Alignment::Center),
                    row![label("Type"), kind("In (relative)", f.relative, Message::EditRelative(true)), kind("At (time of day)", !f.relative, Message::EditRelative(false))]
                        .spacing(6)
                        .align_y(Alignment::Center),
                ]
                .spacing(12);
                if f.relative {
                    form = form.push(row![label("Next in"), field("e.g. 45m, 2h 30m", &f.next_in, Message::EditNextIn)].align_y(Alignment::Center));
                } else {
                    // Repeating reminders only keep a time of day; one-shots keep the full date.
                    let hint = if f.repeating { "e.g. 7:00 AM" } else { "e.g. Oct 24, 2026 6:30 AM" };
                    form = form.push(row![label("At"), field(hint, &f.at, Message::EditAt)].align_y(Alignment::Center));
                }
                form = form.push(row![
                    space().width(110),
                    checkbox(f.repeating).label("Repeating").style(th::checkbox_style(p, false)).on_toggle(Message::EditRepeating)
                ]);
                if f.relative && f.repeating {
                    form = form.push(row![label("Repeat every"), field("e.g. 30m, 2h, 1d", &f.repeat_every, Message::EditRepeatEvery)].align_y(Alignment::Center));
                }
                if let Some(e) = error {
                    form = form.push(text(e.clone()).color(p.danger));
                }
                form.push(buttons(vec![btn("Cancel", ButtonKind::Normal, Message::CloseModal).into(), btn("Save", ButtonKind::Primary, Message::EditSave).into()]))
                    .into()
            }
            Modal::Subtasks(f) => {
                let items = Column::with_children(f.items.iter().enumerate().map(|(i, item)| {
                    row![
                        checkbox(item.checked).label(item.text.as_str()).style(th::checkbox_style(p, false)).on_toggle(move |v| Message::SubtaskCheck(i, v)).width(Length::Fill),
                        button(text("Remove").size(pt(9.0))).padding([4, 8]).style(th::button_style(p, ButtonKind::RowDanger)).on_press(Message::SubtaskRemove(i)),
                    ]
                    .align_y(Alignment::Center)
                    .into()
                }))
                .spacing(6);
                column![
                    heading(&format!("Subtasks: {}", f.title)),
                    text("Subtask progress resets each time this repeating reminder starts a new period.").color(p.muted),
                    container(scrollable(items).height(Length::Shrink)).max_height(260),
                    row![
                        text_input("Add a subtask", &f.new_text).on_input(Message::SubtaskNewText).on_submit(Message::SubtaskAdd).padding([7, 10]).style(th::input_style(p)),
                        btn("Add", ButtonKind::Normal, Message::SubtaskAdd),
                    ]
                    .spacing(8),
                    buttons(vec![btn("Cancel", ButtonKind::Normal, Message::CloseModal).into(), btn("OK", ButtonKind::Primary, Message::SubtasksSave).into()]),
                ]
                .spacing(12)
                .into()
            }
            Modal::History => {
                let rows = Column::with_children(self.store.completed.iter().rev().map(|c| self.completed_row(c)));
                column![
                    heading("History"),
                    container(scrollable(rows).style(th::scroll_style(p))).max_height(420).style(th::panel(p.surface, Some(p.border), 8.0)),
                    buttons(vec![btn("Close", ButtonKind::Primary, Message::CloseModal).into()]),
                ]
                .spacing(12)
                .into()
            }
            Modal::Export(share) => column![
                heading("Export reminders"),
                text("Export string copied to clipboard. Send it to another RemindMe and use Import there."),
                container(scrollable(text(share.clone()).size(pt(9.0)).color(p.muted)).height(Length::Shrink))
                    .max_height(160)
                    .padding(8)
                    .style(th::panel(p.surface, Some(p.border), 6.0)),
                buttons(vec![btn("OK", ButtonKind::Primary, Message::CloseModal).into()]),
            ]
            .spacing(12)
            .into(),
            Modal::Import(content) => column![
                heading("Import reminders"),
                text("Paste export string:").color(p.muted),
                text_editor(content).on_action(Message::ImportEdit).height(140).style(th::editor_style(p)),
                buttons(vec![btn("Cancel", ButtonKind::Normal, Message::CloseModal).into(), btn("Import", ButtonKind::Primary, Message::ImportConfirm).into()]),
            ]
            .spacing(12)
            .into(),
            Modal::UpdateAvailable { tag, release_url, asset } => {
                let (body, action): (String, Element<'a, Message>) = match asset {
                    Some(a) => (
                        format!(
                            "A new version ({tag}) is available.\nCurrent version: {}.\n\nDo you want to download and install it now?{}",
                            remindme::APP_VERSION,
                            if a.is_installer { "" } else { "\n\nNo installer asset was found. RemindMe can download the release package, but installation may require manual steps." }
                        ),
                        btn("Download", ButtonKind::Primary, Message::UpdateDownload).into(),
                    ),
                    None => (
                        format!("A new version ({tag}) is available.\nCurrent version: {}.\n\nOpen the releases page now?", remindme::APP_VERSION),
                        btn("Open releases page", ButtonKind::Primary, Message::OpenUrl(release_url.clone())).into(),
                    ),
                };
                column![heading("Update available"), text(body), buttons(vec![btn("Not now", ButtonKind::Normal, Message::CloseModal).into(), action])]
                    .spacing(14)
                    .into()
            }
            Modal::InstallNow(path) => column![
                heading("Install update"),
                text(format!("Update downloaded to:\n{}\n\nInstall now? RemindMe will close.", path.display())),
                buttons(vec![btn("Later", ButtonKind::Normal, Message::CloseModal).into(), btn("Install", ButtonKind::Primary, Message::InstallConfirmed).into()]),
            ]
            .spacing(14)
            .into(),
        }
    }

    // ---------- popup ----------

    fn popup_view<'a>(&'a self, popup: &'a Popup) -> Element<'a, Message> {
        let p = self.palette;
        let due_label = if popup.due.date_naive() == self.now.date_naive() {
            format!("Due today at {}", hm(&popup.due))
        } else {
            format!(
                "Due {} at {}",
                popup.due.format("%a, %b %-d"),
                hm(&popup.due)
            )
        };
        let overdue = (self.now - popup.due).num_seconds();
        let minutes = overdue / 60;
        let status = if overdue < 60 {
            "Due now".to_string()
        } else if minutes < 60 {
            format!("Due {minutes} min ago")
        } else {
            format!("Due {} h {} min ago", minutes / 60, minutes % 60)
        };
        // The board card: the one loud moment in the app.
        let card = container(
            column![
                text(&popup.title)
                    .size(pt(22.0))
                    .font(th::SEMIBOLD)
                    .color(p.board_text),
                text(due_label).size(pt(11.0)).color(p.board_muted)
            ]
            .spacing(6),
        )
        .padding(Padding {
            top: 22.0,
            right: 28.0,
            bottom: 24.0,
            left: 28.0,
        })
        .width(Length::Fill)
        .style(th::panel(p.board, None, 0.0));
        let big = |label: &'static str, kind: ButtonKind, m: Message| {
            button(text(label).size(pt(12.0)))
                .padding([10, 18])
                .style(th::button_style(p, kind))
                .on_press(m)
        };
        let body = column![
            text(status)
                .size(pt(20.0))
                .font(th::SEMIBOLD)
                .color(p.due_text),
            text(format!("It's {}", hm(&self.now)))
                .size(pt(10.0))
                .color(p.muted),
            space().height(Length::Fill),
            row![
                space::horizontal(),
                big("Snooze 5 min", ButtonKind::Normal, Message::PopupSnooze),
                big("Done", ButtonKind::Primary, Message::PopupDone)
            ]
            .spacing(10),
        ]
        .spacing(8)
        .padding(Padding {
            top: 18.0,
            right: 28.0,
            bottom: 22.0,
            left: 28.0,
        })
        .height(Length::Fill);
        column![card, body].into()
    }

    // ---------- overlay ----------

    fn overlay_rows(&self) -> impl Iterator<Item = (usize, &Reminder)> {
        self.store.items.iter().take(OVERLAY_MAX_ROWS).enumerate()
    }

    fn overlay_row_height(r: &Reminder, is_next: bool) -> f32 {
        let base = if is_next { 52.0 } else { 36.0 };
        base + if r.checklist.is_empty() { 0.0 } else { 15.0 }
    }

    /// Exact height of the overlay's content, so the window hugs its rows.
    fn overlay_content_height(&self) -> f32 {
        if self.store.items.is_empty() {
            return 44.0;
        }
        let rows: f32 = self
            .overlay_rows()
            .map(|(i, r)| Self::overlay_row_height(r, i == 0) + if i > 0 { 1.0 } else { 0.0 })
            .sum();
        let more = if self.store.items.len() > OVERLAY_MAX_ROWS {
            26.0
        } else {
            0.0
        };
        rows + more + 6.0
    }

    fn overlay_view(&self) -> Element<'_, Message> {
        let p = self.palette;
        let content_w = OVERLAY_WIDTH - 2.0 - 4.0 - 14.0 - 14.0 - 6.0 - 1.0;
        let timer_w = (content_w * 0.4).clamp(56.0, 132.0);
        let title_w = (content_w - timer_w - 8.0).max(64.0);
        let meta = |s: String| text(s).size(pt(8.0)).color(p.board_muted);

        let mut col = Column::new();
        if self.store.items.is_empty() {
            col = col.push(
                container(meta("No upcoming reminders".into()))
                    .height(38)
                    .align_y(Alignment::Center),
            );
        }
        for (i, r) in self.overlay_rows() {
            let is_next = i == 0;
            if i > 0 {
                col = col.push(
                    container(space().height(1))
                        .width(Length::Fill)
                        .style(th::panel(p.board_raised, None, 0.0)),
                );
            }
            let title_px = if is_next { 15.0 } else { 13.0 };
            let title = r.title.split_whitespace().collect::<Vec<_>>().join(" ");
            let title = if title.is_empty() {
                "(Untitled)".to_string()
            } else {
                title
            };
            // The next reminder gets two lines; others are one line, ellipsized.
            let lines = if is_next { 2.0 } else { 1.0 };
            let title = elide(&title, (title_w / (title_px * 0.55) * lines) as usize);
            let timer_px = if is_next { 21.0 } else { 15.0 };
            let timer = fit_overlay_countdown((r.next - self.now).num_seconds(), timer_w, timer_px);
            let mut cell = column![
                row![
                    text(title)
                        .size(title_px)
                        .font(if is_next { th::SEMIBOLD } else { Font::DEFAULT })
                        .color(p.board_text)
                        .width(title_w),
                    space::horizontal(),
                    text(timer)
                        .size(timer_px)
                        .font(th::BOLD)
                        .color(if is_next { p.accent } else { p.board_text })
                        .wrapping(text::Wrapping::None),
                ]
                .align_y(Alignment::Center)
            ]
            .spacing(2);
            if !r.checklist.is_empty() {
                cell = cell.push(meta(sched::subtask_progress_text(r)));
            }
            col = col.push(
                container(cell)
                    .height(Self::overlay_row_height(r, is_next))
                    .align_y(Alignment::Center),
            );
        }
        if self.store.items.len() > OVERLAY_MAX_ROWS {
            col = col.push(
                container(meta(format!(
                    "+{} more",
                    self.store.items.len() - OVERLAY_MAX_ROWS
                )))
                .height(26)
                .align_y(Alignment::Center),
            );
        }
        let panel = container(col)
            .padding([2, 14])
            .width(Length::Fill)
            .height(Length::Fill)
            .style(th::panel(p.board, Some(p.board_raised), 10.0));
        // Drag anywhere to move it.
        mouse_area(panel)
            .on_press(Message::DragOverlay)
            .interaction(iced::mouse::Interaction::Grab)
            .into()
    }
}

/// Tray and second-launch requests, delivered as they happen. A plain thread does the blocking
/// recv: a blocked runtime worker would stall shutdown, a detached thread does not.
fn external_events() -> impl iced::futures::Stream<Item = Message> {
    iced::stream::channel(16, async |mut out| {
        std::thread::spawn(move || {
            while let Some(e) = platform::recv() {
                // Full or closed: only closed ends the loop; a full queue drops the request.
                if out
                    .try_send(Message::External(e))
                    .is_err_and(|f| f.is_disconnected())
                {
                    break;
                }
            }
        });
    })
}

fn elide(s: &str, max_chars: usize) -> String {
    if s.chars().count() <= max_chars.max(4) {
        return s.to_string();
    }
    let cut: String = s.chars().take(max_chars.max(4) - 1).collect();
    format!("{}…", cut.trim_end())
}

/// Longest countdown form that fits the timer column at this size.
fn fit_overlay_countdown(seconds: i64, column_px: f32, px: f32) -> String {
    let s = seconds.max(0);
    if s == 0 {
        return "Due now".into();
    }
    let (days, hours, minutes) = (s / 86400, s % 86400 / 3600, s % 3600 / 60);
    let day = if days > 9999 {
        "9999d+".to_string()
    } else {
        format!("{days}d")
    };
    let mut candidates = vec![tf::format_countdown(s)];
    if days > 0 {
        candidates.extend([
            format!("{day} {hours}:{minutes:02}"),
            format!("{day} {hours}h"),
            day,
        ]);
    } else if hours > 0 {
        candidates.extend([format!("{hours}h {minutes}m"), format!("{hours}h")]);
    }
    candidates.push(format!("{}m", (s + 59) / 60));
    let max = (column_px - 6.0).max(24.0);
    let fits = |c: &String| c.chars().count() as f32 * px * 0.58 <= max;
    candidates
        .iter()
        .find(|c| fits(c))
        .unwrap_or(candidates.last().unwrap())
        .clone()
}

/// X11 window managers ignore the level set at creation; set it again once the window is mapped.
fn keep_on_top(open: Task<window::Id>) -> Task<Message> {
    open.then(|id| window::set_level(id, window::Level::AlwaysOnTop))
}

fn text_input_focus<T: Send + 'static>() -> Task<T> {
    iced::widget::operation::focus(INPUT_ID)
}

fn platform_settings() -> window::settings::PlatformSpecific {
    #[allow(unused_mut)]
    let mut s = window::settings::PlatformSpecific::default();
    #[cfg(target_os = "linux")]
    {
        s.application_id = "remindme".into();
    }
    s
}

fn overlay_platform_settings() -> window::settings::PlatformSpecific {
    #[allow(unused_mut)]
    let mut s = platform_settings();
    #[cfg(windows)]
    {
        s.skip_taskbar = true;
    }
    s
}
