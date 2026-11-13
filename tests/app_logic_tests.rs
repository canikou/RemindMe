// SPDX-License-Identifier: MIT
//! Edit-dialog and popup Done/Snooze logic, without any window.

use chrono::{DateTime, Duration, Local};
use remindme::edit::{edit_form, edited_reminder, finish_reminder};
use remindme::model::{Reminder, ScheduleType};
use remindme::schedule::SNOOZE_SECONDS;
use remindme::store::ReminderStore;

fn relative(id: &str, next: DateTime<Local>, repeating: bool) -> Reminder {
    Reminder {
        id: id.into(),
        title: "Water".into(),
        next,
        schedule_type: ScheduleType::Relative,
        interval_seconds: 3600,
        repeating,
        ..Default::default()
    }
}

fn store_with(r: Reminder) -> ReminderStore {
    let mut s = ReminderStore::new();
    s.items.push(r);
    s
}

#[test]
fn title_only_edit_keeps_next_exactly() {
    let t0 = Local::now();
    let r = relative("a", t0 + Duration::seconds(3725), false);
    let mut form = edit_form(&r, t0);
    form.title = "Hydrate".into();
    // The dialog stayed open 90s before Save.
    let saved = edited_reminder(&r, &form, t0 + Duration::seconds(90)).unwrap();
    assert_eq!(saved.title, "Hydrate");
    assert_eq!(saved.next, r.next);
}

#[test]
fn overdue_title_only_edit_does_not_refire() {
    let t0 = Local::now();
    let r = relative("a", t0 - Duration::seconds(600), false);
    let mut form = edit_form(&r, t0);
    form.title = "Hydrate".into();
    let now = t0 + Duration::seconds(5);
    let saved = edited_reminder(&r, &form, now).unwrap();
    assert_eq!(saved.next, r.next);
    // The old behaviour clamped to 1s from now; either way it must not be pushed into the future.
    assert!(saved.next <= now);
}

#[test]
fn schedule_edit_counts_from_when_the_dialog_opened() {
    let t0 = Local::now();
    let r = relative("a", t0 + Duration::seconds(3600), false);
    let mut form = edit_form(&r, t0);
    form.next_in = "2h".into();
    let saved = edited_reminder(&r, &form, t0 + Duration::seconds(90)).unwrap();
    assert_eq!(saved.next, t0 + Duration::hours(2));
    assert_eq!(saved.interval_seconds, 7200);
}

#[test]
fn done_on_one_shot_moves_to_history() {
    let now = Local::now();
    let mut s = store_with(relative("a", now - Duration::seconds(1), false));
    assert!(finish_reminder(&mut s, "a", true, now));
    assert!(s.items.is_empty());
    assert_eq!(s.completed.len(), 1);
    assert_eq!(s.completed[0].title, "Water");
}

#[test]
fn snooze_sets_now_plus_snooze() {
    let now = Local::now();
    let mut s = store_with(relative("a", now - Duration::seconds(1), false));
    assert!(finish_reminder(&mut s, "a", false, now));
    assert_eq!(s.items[0].next, now + Duration::seconds(SNOOZE_SECONDS));
    assert!(s.completed.is_empty());
}

#[test]
fn done_on_repeating_reschedules() {
    let now = Local::now();
    let mut s = store_with(relative("a", now - Duration::seconds(1), true));
    assert!(finish_reminder(&mut s, "a", true, now));
    assert_eq!(s.items.len(), 1);
    assert!(s.items[0].next > now);
    assert!(s.completed.is_empty());
}

#[test]
fn finish_on_missing_reminder_reports_it() {
    let mut s = ReminderStore::new();
    assert!(!finish_reminder(&mut s, "gone", true, Local::now()));
}
