// SPDX-License-Identifier: MIT
//! Data-safety and update-safety regressions: store backups, legacy migration, import limits, update rules.

use chrono::Local;
use remindme::model::ScheduleType;
use remindme::store::ReminderStore;
use remindme::update::{self, Platform};
use serde_json::json;
use std::path::{Path, PathBuf};

const GOOD_DIGEST: &str = "sha256:cc2038bd9ed5fd34eb2cda3c3e51c3d11eedd5c64b33841f1cca1828f146d33a";

fn reminder_json(id: &str, title: &str) -> serde_json::Value {
    json!({"id": id, "title": title, "next_epoch": Local::now().timestamp() + 3600,
           "repeating": false, "schedule": "relative", "interval_seconds": 60,
           "time_of_day": "", "repeat_weekdays_mask": 0})
}

/// Files in `dir` whose name starts with `prefix`.
fn files_with_prefix(dir: &Path, prefix: &str) -> Vec<PathBuf> {
    std::fs::read_dir(dir)
        .unwrap()
        .map(|e| e.unwrap().path())
        .filter(|p| p.file_name().unwrap().to_string_lossy().starts_with(prefix))
        .collect()
}

#[test]
fn corrupt_file_is_moved_aside_not_overwritten() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("reminders.json");
    std::fs::write(&path, "{ this is not json").unwrap();
    let mut store = ReminderStore::with_paths(&path, None);

    let err = store.load().unwrap_err();
    let backups = files_with_prefix(dir.path(), "reminders.json.corrupt-");
    assert_eq!(backups.len(), 1, "{err}");
    assert_eq!(
        std::fs::read_to_string(&backups[0]).unwrap(),
        "{ this is not json"
    );
    assert!(err.contains("corrupt-"), "error names the backup: {err}");

    // The app saves after load failures; that must not touch the backup.
    store.save().unwrap();
    assert_eq!(
        std::fs::read_to_string(&backups[0]).unwrap(),
        "{ this is not json"
    );
}

#[test]
fn file_with_utf8_bom_loads() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("reminders.json");
    let text = json!({"version": 4, "completed": [], "reminders": [reminder_json("a", "Keep me")]});
    std::fs::write(&path, format!("\u{feff}{text}")).unwrap();
    let mut store = ReminderStore::with_paths(&path, None);

    store.load().unwrap();
    assert_eq!(store.items.len(), 1);
    assert!(files_with_prefix(dir.path(), "reminders.json.corrupt-").is_empty());
}

#[test]
fn list_that_is_not_a_list_is_backed_up() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("reminders.json");
    let text = json!({"version": 4, "completed": [], "reminders": {"a": 1}}).to_string();
    std::fs::write(&path, &text).unwrap();
    let mut store = ReminderStore::with_paths(&path, None);

    store.load().unwrap();
    let backups = files_with_prefix(dir.path(), "reminders.json.bak-");
    assert_eq!(backups.len(), 1);
    assert_eq!(std::fs::read_to_string(&backups[0]).unwrap(), text);
}

#[test]
fn dropped_entries_are_backed_up_once_loaded() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("reminders.json");
    let text = json!({"version": 4, "completed": [],
        "reminders": [reminder_json("ok", "Keep me"), reminder_json("bad", "")]})
    .to_string();
    std::fs::write(&path, &text).unwrap();
    let mut store = ReminderStore::with_paths(&path, None);

    store.load().unwrap();
    assert_eq!(store.items.len(), 1);
    let backups = files_with_prefix(dir.path(), "reminders.json.bak-");
    assert_eq!(backups.len(), 1);
    assert_eq!(std::fs::read_to_string(&backups[0]).unwrap(), text);
}

#[test]
fn newer_version_file_is_backed_up_once_loaded() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("reminders.json");
    // The bad entry is dropped too: still only one backup.
    let text = json!({"version": 99, "completed": [],
        "reminders": [reminder_json("ok", "Keep me"), reminder_json("bad", "")]})
    .to_string();
    std::fs::write(&path, &text).unwrap();
    ReminderStore::with_paths(&path, None).load().unwrap();
    let backups = files_with_prefix(dir.path(), "reminders.json.bak-");
    assert_eq!(backups.len(), 1);
    assert_eq!(std::fs::read_to_string(&backups[0]).unwrap(), text);

    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("reminders.json");
    let text =
        json!({"version": 4, "completed": [], "reminders": [reminder_json("ok", "Keep me")]});
    std::fs::write(&path, text.to_string()).unwrap();
    ReminderStore::with_paths(&path, None).load().unwrap();
    assert!(files_with_prefix(dir.path(), "reminders.json.bak-").is_empty());
}

#[test]
fn snoozed_from_round_trips_and_bad_values_are_ignored() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("reminders.json");
    let anchor = Local::now() - chrono::Duration::minutes(5);
    let mut store = ReminderStore::with_paths(&path, None);
    store
        .import_share_string(&json!({"reminders": [reminder_json("", "A")]}).to_string())
        .unwrap();
    store.save().unwrap();
    assert!(
        !std::fs::read_to_string(&path)
            .unwrap()
            .contains("snoozed_from_epoch")
    );
    store.items[0].snoozed_from = Some(anchor);
    store.save().unwrap();

    let mut loaded = ReminderStore::with_paths(&path, None);
    loaded.load().unwrap();
    assert_eq!(
        loaded.items[0].snoozed_from.unwrap().timestamp(),
        anchor.timestamp()
    );

    // Garbage in the field is None, not a dropped entry.
    let mut bad = reminder_json("x", "B");
    bad["snoozed_from_epoch"] = json!("junk");
    std::fs::write(&path, json!({"version": 4, "reminders": [bad]}).to_string()).unwrap();
    loaded.load().unwrap();
    assert_eq!(loaded.items[0].snoozed_from, None);
    assert!(files_with_prefix(dir.path(), "reminders.json.bak-").is_empty());
}

#[test]
fn clean_file_makes_no_backup() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("reminders.json");
    let text =
        json!({"version": 4, "completed": [], "reminders": [reminder_json("ok", "Keep me")]});
    std::fs::write(&path, text.to_string()).unwrap();
    ReminderStore::with_paths(&path, None).load().unwrap();
    assert_eq!(std::fs::read_dir(dir.path()).unwrap().count(), 1);
}

#[test]
fn save_leaves_no_temp_file() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("reminders.json");
    let mut store = ReminderStore::with_paths(&path, None);
    store
        .import_share_string(&json!({"reminders": [reminder_json("", "A")]}).to_string())
        .unwrap();
    store.save().unwrap();
    assert_eq!(std::fs::read_dir(dir.path()).unwrap().count(), 1);
    // A failed rename (target is a directory) reports the io error and cleans up its temp file.
    let blocked = dir.path().join("blocked.json");
    std::fs::create_dir(&blocked).unwrap();
    let mut store = ReminderStore::with_paths(&blocked, None);
    store
        .import_share_string(&json!({"reminders": [reminder_json("", "A")]}).to_string())
        .unwrap();
    let err = store.save().unwrap_err();
    assert!(err.starts_with("Failed to write reminders file: "), "{err}");
    assert!(files_with_prefix(dir.path(), "blocked.json.tmp").is_empty());
}

#[test]
fn legacy_migration_copies_without_leftovers() {
    let dir = tempfile::tempdir().unwrap();
    let legacy = dir.path().join("legacy/reminders.json");
    std::fs::create_dir_all(legacy.parent().unwrap()).unwrap();
    let text =
        json!({"version": 4, "completed": [], "reminders": [reminder_json("old", "Old one")]});
    std::fs::write(&legacy, text.to_string()).unwrap();
    let new = dir.path().join("new/reminders.json");

    let mut store = ReminderStore::with_paths(&new, Some(legacy));
    store.load().unwrap();
    assert_eq!(store.items[0].id, "old");
    assert_eq!(std::fs::read_dir(new.parent().unwrap()).unwrap().count(), 1);
}

#[test]
fn failed_migration_still_shows_legacy_data() {
    let dir = tempfile::tempdir().unwrap();
    let legacy = dir.path().join("legacy.json");
    let text =
        json!({"version": 4, "completed": [], "reminders": [reminder_json("old", "Old one")]});
    std::fs::write(&legacy, text.to_string()).unwrap();
    // A file where the destination folder should be makes the copy impossible.
    let blocker = dir.path().join("blocker");
    std::fs::write(&blocker, "x").unwrap();
    let new = blocker.join("sub/reminders.json");

    let mut store = ReminderStore::with_paths(&new, Some(legacy));
    store.load().unwrap();
    assert_eq!(store.items.len(), 1);
    assert!(!new.exists());
}

fn imported(entry: serde_json::Value) -> ReminderStore {
    let mut store = ReminderStore::default();
    store
        .import_share_string(&json!({"reminders": [entry]}).to_string())
        .unwrap();
    store
}

#[test]
fn out_of_range_interval_does_not_wrap_to_a_flood() {
    let mut e = reminder_json("", "Wrap");
    e["repeating"] = json!(true);
    e["interval_seconds"] = json!(4_294_967_297_i64); // wraps to 1 as i32
    let r = &imported(e).items[0];
    assert_eq!(r.interval_seconds, 0);
}

#[test]
fn imported_short_repeat_is_floored() {
    let mut e = reminder_json("", "Fast");
    e["repeating"] = json!(true);
    e["interval_seconds"] = json!(1);
    let r = &imported(e).items[0];
    assert!(r.schedule_type == ScheduleType::Relative && r.interval_seconds >= 30);
}

#[test]
fn out_of_range_epoch_is_not_1970() {
    let mut e = reminder_json("", "Far");
    e["next_epoch"] = json!(i64::MAX / 2);
    let r = &imported(e).items[0];
    assert!(r.next.timestamp() > 1_000_000_000);
}

fn asset(name: &str, url: &str, digest: Option<&str>) -> serde_json::Value {
    let mut a = json!({"name": name, "browser_download_url": url});
    if let Some(d) = digest {
        a["digest"] = json!(d);
    }
    a
}

#[test]
fn update_assets_need_https_and_full_digest() {
    let pick = |a| update::pick_best_release_asset(&[a], Platform::Windows);
    let name = "RemindMe-9.9.9-setup.exe";
    let url = "https://example.com/s.exe";
    let ok = pick(asset(name, url, Some(GOOD_DIGEST))).unwrap();
    assert_eq!(ok.sha256_hex, GOOD_DIGEST.trim_start_matches("sha256:"));

    assert!(pick(asset(name, "http://example.com/s.exe", Some(GOOD_DIGEST))).is_none());
    assert!(pick(asset(name, url, None)).is_none());
    assert!(pick(asset(name, url, Some("sha256:"))).is_none());
    assert!(pick(asset(name, url, Some("sha256:ABCDEF1234"))).is_none());
    assert!(
        pick(asset(
            name,
            url,
            Some(&format!("sha256:{}", "z".repeat(64)))
        ))
        .is_none()
    );
    assert!(
        pick(asset(
            name,
            url,
            Some(&GOOD_DIGEST.replace("sha256:", "md5:"))
        ))
        .is_none()
    );
}

#[test]
fn unverifiable_asset_does_not_shadow_a_verified_one() {
    let assets = [
        asset(
            "RemindMe-9.9.9-setup.exe",
            "https://example.com/s.exe",
            Some(GOOD_DIGEST),
        ),
        asset(
            "RemindMe-9.9.9.msi",
            "http://example.com/i.msi",
            Some(GOOD_DIGEST),
        ),
        asset(
            "RemindMe-9.9.9-portable.zip",
            "https://example.com/p.zip",
            None,
        ),
    ];
    let best = update::pick_best_release_asset(&assets, Platform::Windows).unwrap();
    assert_eq!(best.name, "RemindMe-9.9.9-setup.exe");
}

#[test]
fn versions_compare_numerically_and_odd_tags_are_errors() {
    assert!(update::is_remote_version_newer("2.0.10", "2.0.9"));
    assert_eq!(update::compare_remote_tag("v2.0.10", "2.0.9"), Ok(true));
    assert_eq!(update::compare_remote_tag("v2.0.1", "2.0.1"), Ok(false));
    for tag in ["v2.1", "2.1.0-rc1", "latest"] {
        let err = update::compare_remote_tag(tag, "2.0.1").unwrap_err();
        assert!(err.contains("Unrecognized version tag"), "{tag}: {err}");
    }
}

#[test]
fn paused_round_trips_and_sorts_after_active() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("reminders.json");
    let mut store = ReminderStore::with_paths(&path, None);
    store
        .import_share_string(
            &json!({"reminders": [reminder_json("", "Soon"), reminder_json("", "Later")]})
                .to_string(),
        )
        .unwrap();
    store.save().unwrap();
    assert!(!std::fs::read_to_string(&path).unwrap().contains("paused"));

    let soon = store.items[0].title.clone();
    store.items[0].paused = true;
    store.sort_soonest_first();
    assert_eq!(store.items.last().unwrap().title, soon, "off goes last");
    assert!(store.active().all(|r| !r.paused));
    store.save().unwrap();

    let mut loaded = ReminderStore::with_paths(&path, None);
    loaded.load().unwrap();
    let r = loaded.items.iter().find(|r| r.title == soon).unwrap();
    assert!(r.paused);
    assert_eq!(loaded.active().count(), 1);
}
