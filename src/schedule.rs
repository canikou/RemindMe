// SPDX-License-Identifier: MIT
//! Scheduling rules and display text shared by the window, overlay and popup.

use crate::model::{CompletedReminder, Reminder, ScheduleType, local, new_id};
use crate::parser::ParseResult;
use crate::store::MAX_COMPLETED_ITEMS;
use crate::time_format::{format_countdown, format_interval_text, hm, hm_time};
use crate::weekday::{is_selected, mask_display_text, normalize_mask};
use chrono::{DateTime, Datelike, Duration, Local, NaiveTime};

pub const DEFAULT_STEP_SECONDS: i32 = 60;
pub const SNOOZE_SECONDS: i64 = 5 * 60;
pub const SHORT_REPEAT_WARNING_SECONDS: i32 = 30;
pub const BOARDING_SECONDS: i64 = 15 * 60;
pub const JOURNEY_HORIZON_SECONDS: i64 = 6 * 60 * 60;

fn step(interval: i32) -> i32 {
    if interval > 0 {
        interval
    } else {
        DEFAULT_STEP_SECONDS
    }
}

/// First `time` strictly after `after` on a selected weekday (empty mask = any day).
pub fn next_at_time_from(time: NaiveTime, after: DateTime<Local>, mask: i32) -> DateTime<Local> {
    let base = after.date_naive();
    for offset in 0..=14 {
        let date = base + Duration::days(offset);
        if !is_selected(mask, date.weekday().number_from_monday() as i32) {
            continue;
        }
        let candidate = local(date, time);
        if candidate > after {
            return candidate;
        }
    }
    local(base + Duration::days(1), time)
}

/// Turns a successful parse into a scheduled reminder (empty id); shared by Add and the live preview.
pub fn reminder_from_parse(p: &ParseResult, now: DateTime<Local>) -> Reminder {
    let mut r = Reminder {
        title: p.title.clone(),
        repeating: p.has_repeat_directive,
        next: now,
        ..Default::default()
    };
    if p.is_relative {
        r.schedule_type = ScheduleType::Relative;
        r.next = now + Duration::seconds(p.duration_seconds as i64);
        r.interval_seconds = if p.has_repeat_directive {
            p.repeat_interval_seconds
        } else {
            p.duration_seconds
        };
        return r;
    }

    let time = p.time_of_day.unwrap_or_default();
    r.time_of_day = Some(time);
    let mask = normalize_mask(p.repeat_weekdays_mask);
    if let Some(date) = p.date {
        r.schedule_type = ScheduleType::AtTimeOfDay;
        r.next = local(date, time);
    } else if r.repeating && mask != 0 {
        r.schedule_type = ScheduleType::AtTimeOfDay;
        r.repeat_weekdays_mask = mask;
        r.next = next_at_time_from(time, now, mask);
    } else if r.repeating && p.repeat_interval_seconds != 86400 {
        r.schedule_type = ScheduleType::Relative;
        r.interval_seconds = p.repeat_interval_seconds;
        // First fire is the next occurrence of the given time; later ones follow the interval.
        r.next = next_at_time_from(time, now, 0);
    } else {
        r.schedule_type = ScheduleType::AtTimeOfDay;
        r.next = next_at_time_from(time, now, 0);
    }
    r
}

/// After Done on a repeating reminder: move to the next period strictly after `now`.
pub fn reschedule_after_acknowledge(r: &mut Reminder, now: DateTime<Local>) {
    if r.schedule_type == ScheduleType::Relative {
        let step = step(r.interval_seconds) as i64;
        if r.next <= now {
            // Jump whole periods at once instead of looping after a long sleep.
            let periods = (now - r.next).num_seconds() / step + 1;
            r.next += Duration::seconds(periods * step);
        }
        return;
    }
    let mask = if r.repeating {
        normalize_mask(r.repeat_weekdays_mask)
    } else {
        0
    };
    r.next = next_at_time_from(r.time_of_day.unwrap_or_default(), now, mask);
}

fn is_same_completed_pattern(c: &CompletedReminder, r: &Reminder) -> bool {
    if c.title != r.title || c.schedule_type != r.schedule_type {
        return false;
    }
    if c.schedule_type == ScheduleType::Relative {
        return step(c.interval_seconds) == step(r.interval_seconds);
    }
    c.time_of_day == r.time_of_day
        && normalize_mask(c.repeat_weekdays_mask) == normalize_mask(r.repeat_weekdays_mask)
}

/// Records a finished one-time reminder; repeats of the same pattern bump a counter instead.
pub fn append_completed(history: &mut Vec<CompletedReminder>, r: &Reminder, now: DateTime<Local>) {
    if r.repeating {
        return;
    }
    if let Some(i) = history.iter().position(|c| is_same_completed_pattern(c, r)) {
        let mut existing = history.remove(i);
        existing.completion_count = existing.completion_count.max(1) + 1;
        existing.completed_at = now;
        history.push(existing);
        return;
    }
    let relative = r.schedule_type == ScheduleType::Relative;
    history.push(CompletedReminder {
        id: new_id(),
        title: r.title.clone(),
        schedule_type: r.schedule_type,
        interval_seconds: if relative {
            step(r.interval_seconds)
        } else {
            0
        },
        time_of_day: if relative { None } else { r.time_of_day },
        repeat_weekdays_mask: if relative {
            0
        } else {
            normalize_mask(r.repeat_weekdays_mask)
        },
        completed_at: now,
        completion_count: 1,
    });
    let excess = history.len().saturating_sub(MAX_COMPLETED_ITEMS);
    history.drain(..excess);
}

pub fn reminder_from_completed(c: &CompletedReminder, now: DateTime<Local>) -> Reminder {
    let mut r = Reminder {
        id: new_id(),
        title: c.title.clone(),
        schedule_type: c.schedule_type,
        next: now,
        ..Default::default()
    };
    if c.schedule_type == ScheduleType::Relative {
        r.interval_seconds = step(c.interval_seconds);
        r.next = now + Duration::seconds(r.interval_seconds as i64);
    } else {
        let time = c.time_of_day.unwrap_or_else(|| now.time());
        r.time_of_day = Some(time);
        r.repeat_weekdays_mask = normalize_mask(c.repeat_weekdays_mask);
        r.next = next_at_time_from(time, now, r.repeat_weekdays_mask);
    }
    r
}

pub fn duration_to_input_text(seconds: i64) -> String {
    let mut rem = seconds.max(1);
    let (days, hours) = (rem / 86400, rem % 86400 / 3600);
    rem %= 3600;
    let (minutes, secs) = (rem / 60, rem % 60);
    let mut parts = Vec::new();
    for (n, u) in [(days, "d"), (hours, "h"), (minutes, "m")] {
        if n > 0 {
            parts.push(format!("{n}{u}"));
        }
    }
    if secs > 0 || parts.is_empty() {
        parts.push(format!("{secs}s"));
    }
    parts.join(" ")
}

pub fn subtask_progress_text(r: &Reminder) -> String {
    if r.checklist.is_empty() {
        return String::new();
    }
    format!("Subtasks {}/{} done", r.checked_count(), r.checklist.len())
}

pub fn due_day_text(when: DateTime<Local>, now: DateTime<Local>) -> String {
    let days = (when.date_naive() - now.date_naive()).num_days();
    match days {
        0 => "Today".into(),
        1 => "Tomorrow".into(),
        2..=6 => when.format("%A").to_string(),
        _ if when.year() == now.year() => when.format("%a, %b %-d").to_string(),
        _ => when.format("%b %-d, %Y").to_string(),
    }
}

pub fn repeats_cell_text(r: &Reminder) -> String {
    if !r.repeating {
        return "Once".into();
    }
    if r.schedule_type == ScheduleType::Relative {
        return match step(r.interval_seconds) {
            86400 => "Daily".into(),
            3600 => "Hourly".into(),
            i => format!("Every {}", format_interval_text(i as i64)),
        };
    }
    let days = mask_display_text(r.repeat_weekdays_mask);
    let mut chars = days.chars();
    match chars.next() {
        Some(first) => first.to_uppercase().chain(chars).collect(),
        None => "Daily".into(),
    }
}

pub fn countdown_cell_text(seconds_left: i64) -> String {
    if seconds_left <= 0 {
        "Due now".into()
    } else {
        format_countdown(seconds_left)
    }
}

pub fn completed_pattern_text(c: &CompletedReminder) -> String {
    if c.schedule_type == ScheduleType::Relative {
        return format!(
            "in {}",
            format_interval_text(step(c.interval_seconds) as i64)
        );
    }
    let time = c.time_of_day.map(hm_time).unwrap_or_default();
    // Completed entries are one-shot reminders, so an empty mask means "no repeat", not "daily".
    if normalize_mask(c.repeat_weekdays_mask) == 0 {
        return format!("at {time}");
    }
    format!("at {time} on {}", mask_display_text(c.repeat_weekdays_mask))
}

pub fn completed_title_text(c: &CompletedReminder) -> String {
    if c.completion_count <= 1 {
        c.title.clone()
    } else {
        format!("{}  (x{})", c.title, c.completion_count)
    }
}

/// How close a reminder is on one shared scale: empty 6+ hours out, full when due.
pub fn journey_progress(r: &Reminder, now: DateTime<Local>) -> f32 {
    let left = (r.next - now).num_seconds() as f32;
    (1.0 - left / JOURNEY_HORIZON_SECONDS as f32).clamp(0.0, 1.0)
}

/// The live hint under the input, as (text, bold) runs.
pub fn input_preview(text: &str, now: DateTime<Local>) -> Vec<(String, bool)> {
    let runs = |parts: &[(&str, bool)]| parts.iter().map(|(t, b)| (t.to_string(), *b)).collect();
    if text.trim().is_empty() {
        return runs(&[
            ("Try ", false),
            ("Drink water in 45m", true),
            (", ", false),
            ("Stand up at 7:00AM every weekday", true),
            (" or ", false),
            ("Dentist on Oct 24 6:30AM", true),
            (".", false),
        ]);
    }
    let Ok(parsed) = crate::parser::parse_input_at(text, now) else {
        return runs(&[
            ("Keep typing: say when with ", false),
            ("in 45m", true),
            (", ", false),
            ("at 7:00AM", true),
            (" or ", false),
            ("on Oct 24 6:30AM", true),
            (".", false),
        ]);
    };
    let r = reminder_from_parse(&parsed, now);
    let mut day = due_day_text(r.next, now);
    if day == "Today" || day == "Tomorrow" {
        day = day.to_lowercase();
    }
    let repeats = if r.repeating {
        format!("repeats {}", repeats_cell_text(&r).to_lowercase())
    } else {
        "one time".into()
    };
    vec![
        ("Understood as ".into(), false),
        (r.title.clone(), true),
        (
            format!(
                "  ·  first {day} at {}  ·  {repeats}. Press Enter to add.",
                hm(&r.next)
            ),
            false,
        ),
    ]
}
