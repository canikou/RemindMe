// SPDX-License-Identifier: MIT

use chrono::{DateTime, Local};

/// No zero-padded leading unit: 18:30, 2:03:48, 3d 20:49:59.
pub fn format_countdown(seconds: i64) -> String {
    let mut s = seconds.max(0);
    let days = s / 86400;
    s %= 86400;
    let (hours, minutes, secs) = (s / 3600, s % 3600 / 60, s % 60);
    if days > 0 {
        format!("{days}d {hours}:{minutes:02}:{secs:02}")
    } else if hours > 0 {
        format!("{hours}:{minutes:02}:{secs:02}")
    } else {
        format!("{minutes}:{secs:02}")
    }
}

pub fn format_due_date_time(dt: &DateTime<Local>) -> String {
    dt.format("%-m/%-d/%Y  %-I:%M %p").to_string()
}

pub fn format_clock_time(dt: &DateTime<Local>) -> String {
    dt.format("%-I:%M:%S %p").to_string()
}

pub fn format_overdue_text(seconds: i64) -> String {
    format!("Overdue {}", format_countdown(seconds))
}

pub fn format_interval_text(seconds: i64) -> String {
    if seconds <= 0 {
        return "0 seconds".into();
    }
    let (n, unit) = if seconds % 86400 == 0 {
        (seconds / 86400, "day")
    } else if seconds % 3600 == 0 {
        (seconds / 3600, "hour")
    } else if seconds % 60 == 0 {
        (seconds / 60, "minute")
    } else {
        (seconds, "second")
    };
    if n == 1 {
        format!("1 {unit}")
    } else {
        format!("{n} {unit}s")
    }
}

/// "7:05 AM"
pub fn hm(dt: &DateTime<Local>) -> String {
    dt.format("%-I:%M %p").to_string()
}

pub fn hm_time(t: chrono::NaiveTime) -> String {
    t.format("%-I:%M %p").to_string()
}
