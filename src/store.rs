// SPDX-License-Identifier: MIT
//! `Documents/RemindMe/reminders.json` and `RM1:` share strings, byte-compatible with 1.x.

use crate::model::{ChecklistItem, CompletedReminder, Reminder, ScheduleType, new_id};
use crate::schedule::SHORT_REPEAT_WARNING_SECONDS;
use crate::weekday::normalize_mask;
use base64::Engine;
use base64::engine::{DecodePaddingMode, GeneralPurpose, GeneralPurposeConfig};
use chrono::{DateTime, Local, NaiveTime, TimeZone};
use serde_json::{Map, Value, json};
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};

const STORAGE_VERSION: i64 = 4;
const SHARE_VERSION: i64 = 1;
pub const MAX_COMPLETED_ITEMS: usize = 50;
const SHARE_PREFIX: &str = "RM1:";
const STORAGE_FILE_NAME: &str = "reminders.json";
pub const APP_FOLDER: &str = "RemindMe";

const SHARE_ENCODE: GeneralPurpose = base64::engine::general_purpose::URL_SAFE_NO_PAD;
const SHARE_DECODE: GeneralPurpose = GeneralPurpose::new(
    &base64::alphabet::URL_SAFE,
    GeneralPurposeConfig::new().with_decode_padding_mode(DecodePaddingMode::Indifferent),
);

/// Qt's AppDataLocation for an app with no organization: `<data dir>/RemindMe`.
pub fn legacy_data_dir() -> Option<PathBuf> {
    dirs::data_dir().map(|d| d.join(APP_FOLDER))
}

/// `Documents/RemindMe`, falling back to the legacy app-data folder.
pub fn documents_app_dir() -> Option<PathBuf> {
    dirs::document_dir()
        .map(|d| d.join(APP_FOLDER))
        .or_else(legacy_data_dir)
}

#[derive(Default)]
pub struct ReminderStore {
    storage_override: Option<PathBuf>,
    legacy_override: Option<PathBuf>,
    /// Set when a file we couldn't read or back up is still on disk; save() then refuses to overwrite it.
    save_blocked: bool,
    pub items: Vec<Reminder>,
    pub completed: Vec<CompletedReminder>,
}

fn schedule_str(t: ScheduleType) -> &'static str {
    match t {
        ScheduleType::AtTimeOfDay => "at_time",
        ScheduleType::Relative => "relative",
    }
}

fn schedule_from(v: Option<&Value>) -> ScheduleType {
    match v.and_then(Value::as_str) {
        Some("at_time") => ScheduleType::AtTimeOfDay,
        _ => ScheduleType::Relative,
    }
}

fn time_str(t: Option<NaiveTime>) -> String {
    t.map(|t| t.format("%H:%M").to_string()).unwrap_or_default()
}

fn time_from(v: Option<&Value>) -> Option<NaiveTime> {
    NaiveTime::parse_from_str(v?.as_str()?, "%H:%M").ok()
}

fn str_of(o: &Map<String, Value>, key: &str) -> String {
    o.get(key)
        .and_then(Value::as_str)
        .unwrap_or_default()
        .to_string()
}

/// JSON numbers as i64 (floats truncate, like Qt).
fn number_of(v: &Value) -> Option<i64> {
    v.as_number()
        .and_then(|n| n.as_i64().or_else(|| n.as_f64().map(|f| f as i64)))
}

/// Qt's toInt(): numbers only, defaulting when missing or out of i32 range.
fn int_of(o: &Map<String, Value>, key: &str, default: i32) -> i32 {
    o.get(key)
        .and_then(number_of)
        .and_then(|v| i32::try_from(v).ok())
        .unwrap_or(default)
}

/// Epoch seconds as a number or numeric string; None when absent or out of range.
fn epoch_of(o: &Map<String, Value>, key: &str) -> Option<DateTime<Local>> {
    let v = o.get(key)?;
    let secs = match v {
        Value::String(s) => s.trim().parse().ok()?,
        _ => number_of(v)?,
    };
    Local.timestamp_opt(secs, 0).single()
}

fn reminder_to_json(r: &Reminder) -> Value {
    let checklist: Vec<Value> = r
        .checklist
        .iter()
        .filter(|i| !i.text.trim().is_empty())
        .map(|i| json!({ "text": i.text.trim(), "checked": i.checked }))
        .collect();
    json!({
        "id": r.id,
        "title": r.title,
        "next_epoch": r.next.timestamp(),
        "repeating": r.repeating,
        "schedule": schedule_str(r.schedule_type),
        "interval_seconds": r.interval_seconds,
        "time_of_day": time_str(r.time_of_day),
        "repeat_weekdays_mask": normalize_mask(r.repeat_weekdays_mask),
        "checklist_items": checklist,
    })
}

fn reminder_from_json(v: &Value, require_id: bool) -> Option<Reminder> {
    let o = v.as_object()?;
    let checklist = o
        .get("checklist_items")
        .and_then(Value::as_array)
        .map(|items| {
            items
                .iter()
                .filter_map(Value::as_object)
                .filter_map(|i| {
                    let text = str_of(i, "text").trim().to_string();
                    (!text.is_empty()).then(|| ChecklistItem {
                        text,
                        checked: i.get("checked").and_then(Value::as_bool).unwrap_or(false),
                    })
                })
                .collect()
        })
        .unwrap_or_default();
    let mut r = Reminder {
        id: str_of(o, "id"),
        title: str_of(o, "title"),
        next: epoch_of(o, "next_epoch").unwrap_or_else(Local::now),
        schedule_type: schedule_from(o.get("schedule")),
        interval_seconds: int_of(o, "interval_seconds", 0),
        time_of_day: time_from(o.get("time_of_day")),
        repeat_weekdays_mask: normalize_mask(int_of(o, "repeat_weekdays_mask", 0)),
        repeating: o.get("repeating").and_then(Value::as_bool).unwrap_or(false),
        checklist,
    };
    r.enforce_checklist_constraints();
    if (require_id && r.id.is_empty()) || r.title.is_empty() {
        return None;
    }
    Some(r)
}

fn completed_to_json(c: &CompletedReminder) -> Value {
    json!({
        "id": c.id,
        "title": c.title,
        "schedule": schedule_str(c.schedule_type),
        "interval_seconds": c.interval_seconds,
        "time_of_day": time_str(c.time_of_day),
        "repeat_weekdays_mask": normalize_mask(c.repeat_weekdays_mask),
        "completed_epoch": c.completed_at.timestamp(),
        "completion_count": c.completion_count,
    })
}

fn completed_from_json(v: &Value) -> Option<CompletedReminder> {
    let o = v.as_object()?;
    let c = CompletedReminder {
        id: str_of(o, "id"),
        title: str_of(o, "title"),
        schedule_type: schedule_from(o.get("schedule")),
        interval_seconds: int_of(o, "interval_seconds", 0),
        time_of_day: time_from(o.get("time_of_day")),
        repeat_weekdays_mask: normalize_mask(int_of(o, "repeat_weekdays_mask", 0)),
        completed_at: epoch_of(o, "completed_epoch").unwrap_or_else(Local::now),
        completion_count: int_of(o, "completion_count", 1).max(1),
    };
    (!c.id.is_empty() && !c.title.is_empty()).then_some(c)
}

fn migrate_legacy_if_needed(legacy: &Path, new: &Path) -> Result<(), String> {
    if legacy == new || new.exists() || !legacy.exists() {
        return Ok(());
    }
    if let Some(parent) = new.parent() {
        fs::create_dir_all(parent).map_err(|e| {
            format!("Failed to create destination folder for reminders migration: {e}")
        })?;
    }
    // Copy beside the target and rename, so a crash never leaves a partial file at the final path.
    let tmp = new.with_extension("json.migrating");
    fs::copy(legacy, &tmp)
        .and_then(|_| fs::rename(&tmp, new))
        .map_err(|e| {
            let _ = fs::remove_file(&tmp);
            format!("Failed to migrate reminders data to the Documents folder: {e}")
        })
}

/// `reminders.json` -> `reminders.json.<tag>-<unix secs>` beside it.
fn side_path(path: &Path, tag: &str) -> PathBuf {
    let mut name = path.as_os_str().to_owned();
    name.push(format!(".{tag}-{}", chrono::Utc::now().timestamp()));
    PathBuf::from(name)
}

impl ReminderStore {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn with_paths(storage: impl Into<PathBuf>, legacy: Option<PathBuf>) -> Self {
        Self {
            storage_override: Some(storage.into()),
            legacy_override: legacy,
            ..Default::default()
        }
    }

    pub fn storage_path(&self) -> PathBuf {
        self.storage_override.clone().unwrap_or_else(|| {
            documents_app_dir()
                .unwrap_or_else(|| PathBuf::from("."))
                .join(STORAGE_FILE_NAME)
        })
    }

    pub fn legacy_storage_path(&self) -> Option<PathBuf> {
        if self.storage_override.is_some() {
            return self.legacy_override.clone();
        }
        legacy_data_dir().map(|d| d.join(STORAGE_FILE_NAME))
    }

    pub fn load(&mut self) -> Result<(), String> {
        self.items.clear();
        self.completed.clear();
        self.save_blocked = false;
        let path = self.storage_path();
        let mut read_path = path.clone();
        if let Some(legacy) = self.legacy_storage_path()
            && migrate_legacy_if_needed(&legacy, &path).is_err()
        {
            // Show the old data rather than nothing; the first save then creates the new file from it.
            read_path = legacy;
        }
        if !read_path.exists() {
            return Ok(()); // first run
        }
        let parsed = fs::read(&read_path)
            .map_err(|e| format!("Failed to read reminders file: {e}"))
            .and_then(|bytes| match serde_json::from_slice::<Value>(&bytes) {
                Ok(Value::Object(root)) => Ok(root),
                _ => Err("Reminders file is corrupted (not a JSON object).".to_string()),
            });
        let root = match parsed {
            Ok(root) => root,
            Err(why) => {
                // The next save would overwrite this file with an empty list: move it aside first.
                let backup = side_path(&read_path, "corrupt");
                let kept = fs::rename(&read_path, &backup).is_ok()
                    || fs::copy(&read_path, &backup).is_ok();
                self.save_blocked = !kept;
                return Err(if kept {
                    format!("{why}\nThe original was kept as {}", backup.display())
                } else {
                    format!("{why}\nCould not back it up, so changes will not be saved.")
                });
            }
        };
        let mut dropped = 0;
        if let Some(arr) = root.get("reminders").and_then(Value::as_array) {
            self.items = arr
                .iter()
                .filter_map(|v| reminder_from_json(v, true))
                .collect();
            dropped += arr.len() - self.items.len();
        }
        if let Some(arr) = root.get("completed").and_then(Value::as_array) {
            self.completed = arr.iter().filter_map(completed_from_json).collect();
            dropped += arr.len() - self.completed.len();
        }
        let excess = self.completed.len().saturating_sub(MAX_COMPLETED_ITEMS);
        self.completed.drain(..excess);
        self.sort_soonest_first();
        if dropped > 0 {
            // The next save drops these entries for good; keep the original.
            let backup = side_path(&read_path, "bak");
            if let Err(e) = fs::copy(&read_path, &backup) {
                self.save_blocked = true;
                return Err(format!(
                    "{dropped} unreadable entries were skipped and could not be backed up ({e}), so changes will not be saved."
                ));
            }
        }
        Ok(())
    }

    pub fn save(&self) -> Result<(), String> {
        if self.save_blocked {
            return Err("Not saving: the existing reminders file could not be read or backed up, and saving would overwrite it.".into());
        }
        let path = self.storage_path();
        if let Some(parent) = path.parent() {
            let _ = fs::create_dir_all(parent);
        }
        let root = json!({
            "version": STORAGE_VERSION,
            "reminders": self.items.iter().map(reminder_to_json).collect::<Vec<_>>(),
            "completed": self.completed.iter().map(completed_to_json).collect::<Vec<_>>(),
        });
        let mut text = serde_json::to_string_pretty(&root).map_err(|e| e.to_string())?;
        text.push('\n');
        // Write, flush to disk, then rename, so a crash mid-save never leaves a truncated file.
        let tmp = path.with_extension("json.tmp");
        fs::File::create(&tmp)
            .and_then(|mut f| {
                f.write_all(text.as_bytes())?;
                f.sync_all()
            })
            .and_then(|_| fs::rename(&tmp, &path))
            .map_err(|e| {
                let _ = fs::remove_file(&tmp);
                format!("Failed to write reminders file: {e}")
            })
    }

    pub fn export_share_string(&self) -> String {
        let root = json!({
            "format": "remindme_share",
            "version": SHARE_VERSION,
            "reminders": self.items.iter().map(reminder_to_json).collect::<Vec<_>>(),
        });
        format!("{SHARE_PREFIX}{}", SHARE_ENCODE.encode(root.to_string()))
    }

    /// Accepts `RM1:<base64url json>` or the raw JSON. Imported reminders get fresh ids.
    pub fn import_share_string(&mut self, share: &str) -> Result<usize, String> {
        let trimmed = share.trim();
        if trimmed.is_empty() {
            return Err("Import string is empty.".into());
        }
        let raw = match trimmed.strip_prefix(SHARE_PREFIX) {
            Some(encoded) => {
                let cleaned: String = encoded.chars().filter(|c| !c.is_whitespace()).collect();
                SHARE_DECODE
                    .decode(cleaned)
                    .ok()
                    .filter(|b| !b.is_empty())
                    .ok_or("Import string is not valid RemindMe share data.")?
            }
            None => trimmed.as_bytes().to_vec(),
        };
        let root: Value =
            serde_json::from_slice(&raw).map_err(|_| "Import string is not valid JSON data.")?;
        let root = root
            .as_object()
            .ok_or("Import string is not valid JSON data.")?;
        let arr = root
            .get("reminders")
            .and_then(Value::as_array)
            .filter(|a| !a.is_empty());
        let arr = arr.ok_or("Import data does not contain reminders.")?;

        let mut count = 0;
        for v in arr {
            if let Some(mut r) = reminder_from_json(v, false) {
                r.id = new_id();
                // Imports skip the app's short-repeat confirmation, so floor the interval here.
                if r.repeating
                    && r.schedule_type == ScheduleType::Relative
                    && (1..SHORT_REPEAT_WARNING_SECONDS).contains(&r.interval_seconds)
                {
                    r.interval_seconds = SHORT_REPEAT_WARNING_SECONDS;
                }
                r.enforce_checklist_constraints();
                self.items.push(r);
                count += 1;
            }
        }
        if count == 0 {
            return Err("Import string had no valid reminders.".into());
        }
        self.sort_soonest_first();
        Ok(count)
    }

    pub fn sort_soonest_first(&mut self) {
        self.items.sort_by_key(|r| r.next);
    }
}
