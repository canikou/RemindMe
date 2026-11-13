// SPDX-License-Identifier: MIT
//! Pure logic behind the edit dialog and the due popup's Done/Snooze. No UI code lives here.

use crate::model::{Reminder, ScheduleType, local};
use crate::parser;
use crate::schedule::{self as sched, SNOOZE_SECONDS};
use crate::store::ReminderStore;
use crate::time_format as tf;
use chrono::{DateTime, Local};

/// The schedule fields as they were when the dialog opened; equal means the schedule is untouched.
type ScheduleKey = (bool, String, String, bool);

pub struct EditForm {
    pub id: String,
    pub title: String,
    pub relative: bool,
    pub next_in: String,
    pub at: String,
    pub repeating: bool,
    pub repeat_every: String,
    /// When the dialog opened: "Next in" counts from here, not from when Save is pressed.
    pub opened: DateTime<Local>,
    initial: ScheduleKey,
}

impl EditForm {
    fn schedule_key(&self) -> ScheduleKey {
        (
            self.relative,
            self.next_in.clone(),
            self.at.clone(),
            self.repeating,
        )
    }
}

pub fn edit_form(r: &Reminder, now: DateTime<Local>) -> EditForm {
    let next_in = (r.next - now).num_seconds().max(1);
    let repeat = if r.interval_seconds > 0 {
        r.interval_seconds as i64
    } else {
        next_in
    };
    let time = r.time_of_day.unwrap_or(r.next.time());
    let mut form = EditForm {
        id: r.id.clone(),
        title: r.title.clone(),
        relative: r.schedule_type == ScheduleType::Relative,
        next_in: sched::duration_to_input_text(next_in),
        at: if r.repeating {
            tf::hm_time(time)
        } else {
            format!("{} {}", r.next.format("%b %-d, %Y"), tf::hm_time(time))
        },
        repeating: r.repeating,
        repeat_every: sched::duration_to_input_text(repeat),
        opened: now,
        initial: (false, String::new(), String::new(), false),
    };
    form.initial = form.schedule_key();
    form
}

/// Applies the edit dialog to a copy of the reminder, or explains what's wrong.
pub fn edited_reminder(
    original: &Reminder,
    f: &EditForm,
    now: DateTime<Local>,
) -> Result<Reminder, String> {
    let mut r = original.clone();
    r.title = f.title.trim().to_string();
    if r.title.is_empty() {
        return Err("Reminder title cannot be empty.".into());
    }
    r.repeating = f.repeating;
    if f.relative {
        let delay = parser::parse_duration_to_seconds(f.next_in.trim())?;
        r.schedule_type = ScheduleType::Relative;
        r.next = f.opened + chrono::Duration::seconds(delay as i64);
        r.repeat_weekdays_mask = 0;
        r.interval_seconds = if !f.repeating || f.repeat_every.trim().is_empty() {
            delay
        } else {
            parser::parse_duration_to_seconds(f.repeat_every.trim())?
        };
    } else {
        r.schedule_type = ScheduleType::AtTimeOfDay;
        r.interval_seconds = 0;
        if f.repeating {
            let time = parser::parse_time_of_day(f.at.trim())?;
            r.time_of_day = Some(time);
            r.next = sched::next_at_time_from(time, now, r.repeat_weekdays_mask);
        } else {
            r.repeat_weekdays_mask = 0;
            // A full date, or just a time meaning its next occurrence.
            let (next, time) = match parser::parse_date_time(f.at.trim(), now) {
                Ok((date, time)) => (local(date, time), time),
                Err(date_err) => match parser::parse_time_of_day(f.at.trim()) {
                    Ok(time) => (sched::next_at_time_from(time, now, 0), time),
                    Err(_) => return Err(date_err),
                },
            };
            r.time_of_day = Some(time);
            r.next = next;
        }
    }
    // The prefilled fields are lossy (remaining seconds, minute-rounded times): untouched keeps the exact due time.
    if f.schedule_key() == f.initial {
        r.next = original.next;
    }
    r.enforce_checklist_constraints();
    Ok(r)
}

/// Done (`done`) or Snooze on the reminder a popup showed. Returns false if it no longer exists.
pub fn finish_reminder(
    store: &mut ReminderStore,
    id: &str,
    done: bool,
    now: DateTime<Local>,
) -> bool {
    let Some(i) = store.items.iter().position(|r| r.id == id) else {
        return false;
    };
    if !done {
        store.items[i].next = now + chrono::Duration::seconds(SNOOZE_SECONDS);
    } else if !store.items[i].repeating {
        let r = store.items.remove(i);
        sched::append_completed(&mut store.completed, &r, now);
    } else {
        let r = &mut store.items[i];
        r.reset_checklist();
        sched::reschedule_after_acknowledge(r, now);
    }
    true
}
