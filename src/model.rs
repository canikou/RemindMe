// SPDX-License-Identifier: MIT

use chrono::{DateTime, Local, LocalResult, NaiveDate, NaiveDateTime, NaiveTime, TimeZone};

#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub enum ScheduleType {
    #[default]
    Relative, // "in 5h23m"
    AtTimeOfDay, // "at 7:00AM"
}

#[derive(Clone, Debug, PartialEq, Default)]
pub struct ChecklistItem {
    pub text: String,
    pub checked: bool,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Reminder {
    pub id: String,
    pub title: String,
    pub next: DateTime<Local>,
    pub schedule_type: ScheduleType,
    /// For "in ..."; also the period of repeating relative reminders.
    pub interval_seconds: i32,
    /// For "at ..."; None when unset.
    pub time_of_day: Option<NaiveTime>,
    /// Monday..Sunday bits for weekday repeats.
    pub repeat_weekdays_mask: i32,
    pub repeating: bool,
    pub checklist: Vec<ChecklistItem>,
}

impl Default for Reminder {
    fn default() -> Self {
        Self {
            id: String::new(),
            title: String::new(),
            next: Local::now(),
            schedule_type: ScheduleType::Relative,
            interval_seconds: 0,
            time_of_day: None,
            repeat_weekdays_mask: 0,
            repeating: false,
            checklist: Vec::new(),
        }
    }
}

impl Reminder {
    pub fn reset_checklist(&mut self) {
        self.checklist.iter_mut().for_each(|i| i.checked = false);
    }

    pub fn checked_count(&self) -> usize {
        self.checklist.iter().filter(|i| i.checked).count()
    }

    /// Subtasks exist only on repeating reminders.
    pub fn enforce_checklist_constraints(&mut self) {
        if !self.repeating {
            self.checklist.clear();
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct CompletedReminder {
    pub id: String,
    pub title: String,
    pub schedule_type: ScheduleType,
    pub interval_seconds: i32,
    pub time_of_day: Option<NaiveTime>,
    pub repeat_weekdays_mask: i32,
    pub completed_at: DateTime<Local>,
    pub completion_count: i32,
}

pub fn new_id() -> String {
    uuid::Uuid::new_v4().to_string()
}

/// Local wall-clock date+time to an instant. DST gaps move forward an hour; overlaps take the earlier one.
pub fn local(date: NaiveDate, time: NaiveTime) -> DateTime<Local> {
    let naive = NaiveDateTime::new(date, time);
    earlier(Local.from_local_datetime(&naive))
        .or_else(|| earlier(Local.from_local_datetime(&(naive + chrono::Duration::hours(1)))))
        .unwrap_or_else(|| Local.from_utc_datetime(&naive))
}

/// The earlier instant of an ambiguous local time. Not `earliest()`: chrono 0.4.45 on Unix
/// lists the later instant first (01:30 EST before 01:30 EDT on the US fall-back night).
pub fn earlier<Tz: TimeZone>(r: LocalResult<DateTime<Tz>>) -> Option<DateTime<Tz>> {
    match r {
        LocalResult::Single(t) => Some(t),
        LocalResult::Ambiguous(a, b) => Some(a.min(b)),
        LocalResult::None => None,
    }
}
