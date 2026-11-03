// SPDX-License-Identifier: MIT
//! `Documents/RemindMe/reminders.json` and `RM1:` share strings, byte-compatible with 1.x.

use crate::model::{ChecklistItem, CompletedReminder, Reminder, ScheduleType, new_id};
use crate::weekday::normalize_mask;
use base64::Engine;
use base64::engine::{DecodePaddingMode, GeneralPurpose, GeneralPurposeConfig};
use chrono::{DateTime, Local, NaiveTime, TimeZone};
use serde_json::{Map, Value, json};
use std::fs;
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

/// Qt's toInt(): numbers only, defaulting otherwise.
fn int_of(o: &Map<String, Value>, key: &str, default: i32) -> i32 {
    match o.get(key) {
        Some(Value::Number(n)) => n
            .as_i64()
            .or_else(|| n.as_f64().map(|f| f as i64))
            .map_or(default, |v| v as i32),
        _ => default,
    }
}

fn epoch_of(o: &Map<String, Value>, key: &str) -> Option<DateTime<Local>> {
    let secs = match o.get(key)? {
        Value::Number(n) => n.as_i64().or_else(|| n.as_f64().map(|f| f as i64))?,
        Value::String(s) => s.trim().parse().ok()?,
        _ => return None,
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
        next: epoch_of(o, "next_epoch").unwrap_or_else(|| Local.timestamp_opt(0, 0).unwrap()),
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
        completed_at: epoch_of(o, "completed_epoch")
            .unwrap_or_else(|| Local.timestamp_opt(0, 0).unwrap()),
        completion_count: int_of(o, "completion_count", 1).max(1),
    };
    (!c.id.is_empty() && !c.title.is_empty()).then_some(c)
}

fn migrate_legacy_if_needed(legacy: &Path, new: &Path) -> Result<(), String> {
    if legacy == new || new.exists() || !legacy.exists() {
        return Ok(());
    }
    if let Some(parent) = new.parent() {
        fs::create_dir_all(parent)
            .map_err(|_| "Failed to create destination folder for reminders migration.")?;
    }
    fs::copy(legacy, new)
        .map(|_| ())
        .map_err(|_| "Failed to migrate reminders data to the Documents folder.".into())
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
        let path = self.storage_path();
        if let Some(legacy) = self.legacy_storage_path() {
            migrate_legacy_if_needed(&legacy, &path)?;
        }
        if !path.exists() {
            return Ok(()); // first run
        }
        let bytes = fs::read(&path).map_err(|_| "Failed to open reminders file for reading.")?;
        let root: Value = serde_json::from_slice(&bytes).unwrap_or(Value::Null);
        let Some(root) = root.as_object() else {
            return Err("Reminders file is corrupted (not a JSON object).".into());
        };
        if let Some(arr) = root.get("reminders").and_then(Value::as_array) {
            self.items = arr
                .iter()
                .filter_map(|v| reminder_from_json(v, true))
                .collect();
        }
        if let Some(arr) = root.get("completed").and_then(Value::as_array) {
            self.completed = arr.iter().filter_map(completed_from_json).collect();
        }
        let excess = self.completed.len().saturating_sub(MAX_COMPLETED_ITEMS);
        self.completed.drain(..excess);
        self.sort_soonest_first();
        Ok(())
    }

    pub fn save(&self) -> Result<(), String> {
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
        // Write then rename, so a crash mid-save never leaves a truncated file.
        let tmp = path.with_extension("json.tmp");
        fs::write(&tmp, text)
            .and_then(|_| fs::rename(&tmp, &path))
            .map_err(|_| "Failed to open reminders file for writing.".into())
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
