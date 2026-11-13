// SPDX-License-Identifier: MIT
//! Port of the 1.x `core_tests.cpp`, plus the 2.0 compatibility checks.

use chrono::{DateTime, Datelike, Duration, Local, NaiveDate, NaiveTime, TimeZone};
use remindme::model::{ChecklistItem, CompletedReminder, Reminder, ScheduleType};
use remindme::parser::{self, ParseResult};
use remindme::schedule;
use remindme::store::{MAX_COMPLETED_ITEMS, ReminderStore};
use remindme::update::{self, Platform};
use remindme::weekday::{self, weekday_bit as bit};
use serde_json::json;

fn t(h: u32, m: u32) -> Option<NaiveTime> {
    NaiveTime::from_hms_opt(h, m, 0)
}

fn d(y: i32, m: u32, day: u32) -> Option<NaiveDate> {
    NaiveDate::from_ymd_opt(y, m, day)
}

/// A Wednesday morning; every parse in these tests runs against it, never the wall clock.
fn fixed_now() -> DateTime<Local> {
    Local.with_ymd_and_hms(2030, 6, 12, 10, 0, 0).unwrap()
}

fn parse(input: &str) -> Result<ParseResult, String> {
    parser::parse_input_at(input, fixed_now())
}

fn mask_of(input: &str) -> i32 {
    let r = parse(input).unwrap_or_else(|e| panic!("{input}: {e}"));
    assert!(
        !r.is_relative && r.has_repeat_directive && r.repeat_interval_seconds == 0,
        "{input}"
    );
    r.repeat_weekdays_mask
}

#[test]
fn parse_relative() {
    let r = parse("Drink water in 45m").unwrap();
    assert!(r.is_relative);
    assert_eq!(
        (r.title.as_str(), r.duration_seconds),
        ("Drink water", 45 * 60)
    );
}

#[test]
fn parse_at_time() {
    let r = parse("Stand up at 7:00AM").unwrap();
    assert!(!r.is_relative);
    assert_eq!((r.title.as_str(), r.time_of_day), ("Stand up", t(7, 0)));
}

#[test]
fn parse_date_time() {
    let a = parse("reinforce on october 24, 2099 6:30AM").unwrap();
    assert_eq!(
        (a.title.as_str(), a.date, a.time_of_day, a.is_relative),
        ("reinforce", d(2099, 10, 24), t(6, 30), false)
    );

    let b = parse("Pay rent at 6:30PM on 2099-01-05").unwrap();
    assert_eq!(
        (b.title.as_str(), b.date, b.time_of_day),
        ("Pay rent", d(2099, 1, 5), t(18, 30))
    );

    let c = parse("Party at 5 Dec 2099 8pm").unwrap();
    assert_eq!((c.date, c.time_of_day), (d(2099, 12, 5), t(20, 0)));

    let tomorrow = Some(fixed_now().date_naive() + Duration::days(1));
    for (input, title, time) in [
        ("Water plants tomorrow", "Water plants", t(9, 0)),
        ("Call mom tomorrow at 7pm", "Call mom", t(19, 0)),
        ("Call mom at 7pm tomorrow", "Call mom", t(19, 0)),
        ("Turn on lights tomorrow", "Turn on lights", t(9, 0)),
    ] {
        let r = parse(input).unwrap();
        assert_eq!(
            (r.title.as_str(), r.date, r.time_of_day),
            (title, tomorrow, time),
            "{input}"
        );
    }

    // Weekdays mean the next time that weekday comes round, strictly in the future.
    for (input, title, dow, time) in [
        ("vacuum room on sunday 8 AM", "vacuum room", 7, t(8, 0)),
        ("Call mom on Friday at 7pm", "Call mom", 5, t(19, 0)),
        ("Call mom wed at 7pm", "Call mom", 3, t(19, 0)),
        ("Pay rent at 9am on next mon", "Pay rent", 1, t(9, 0)),
    ] {
        let now = fixed_now();
        let r = parse(input).unwrap();
        let when = remindme::model::local(r.date.unwrap(), r.time_of_day.unwrap());
        assert_eq!(r.title, title, "{input}");
        assert_eq!(
            r.date.unwrap().weekday().number_from_monday(),
            dow,
            "{input}"
        );
        assert_eq!(r.time_of_day, time, "{input}");
        assert!(when > now && when <= now + Duration::days(7), "{input}");
    }

    let l = parse("Pay rent on Sunday, Oct 26 2099 9am").unwrap();
    assert_eq!((l.date, l.time_of_day), (d(2099, 10, 26), t(9, 0)));

    let m = parse("Turn on lights at 7pm").unwrap();
    assert_eq!((m.title.as_str(), m.date), ("Turn on lights", None));

    for bad in [
        "old on january 1, 2000 6:30AM",
        "x on feb 30, 2099 6:30AM",
        "x on october 24, 2099",
        "x on october 24, 2099 6:30AM every day",
    ] {
        assert!(parse(bad).is_err(), "{bad}");
    }
}

#[test]
fn parse_time_am_pm_boundaries() {
    assert_eq!(
        parse("Midnight task at 12:00AM").unwrap().time_of_day,
        t(0, 0)
    );
    assert_eq!(parse("Noon task at 12:00PM").unwrap().time_of_day, t(12, 0));
    assert!(!parse("Invalid task at 00:30PM").unwrap_err().is_empty());
}

#[test]
fn parse_repeat_interval() {
    let r = parse("Hydrate in 10m every 2 hours").unwrap();
    assert!(r.has_repeat_directive);
    assert_eq!(r.repeat_interval_seconds, 2 * 3600);
}

#[test]
fn parse_repeat_weekdays() {
    let weekdays = bit(1) | bit(2) | bit(3) | bit(4) | bit(5);
    assert_eq!(mask_of("Reminder at 7:00AM every Saturday"), bit(6));
    assert_eq!(mask_of("Reminder at 7:00AM every Saturday, etc."), bit(6));
    assert_eq!(mask_of("Reminder at 7:00AM every weekdays"), weekdays);
    assert_eq!(
        mask_of("Reminder at 7:00AM every weekends"),
        bit(6) | bit(7)
    );
    assert_eq!(
        mask_of("Reminder at 7:00AM every Mon Wed Fri"),
        bit(1) | bit(3) | bit(5)
    );
    assert_eq!(mask_of("Reminder at 7:00AM every Mon-Fri"), weekdays);
    assert_eq!(
        mask_of("Reminder at 7:00AM every Monday through Friday"),
        weekdays
    );
    assert_eq!(
        mask_of("Reminder at 7:00AM every Tue thru Thu"),
        bit(2) | bit(3) | bit(4)
    );
    assert_eq!(
        mask_of("Reminder at 7:00AM every from monday to friday"),
        weekdays
    );
    assert_eq!(
        mask_of("Reminder at 7:00AM every Fri-Mon"),
        bit(5) | bit(6) | bit(7) | bit(1)
    );
    assert_eq!(mask_of("Reminder at 7:00AM every Mon\u{2013}Fri"), weekdays);
    assert_eq!(
        mask_of("Reminder at 7:00AM every Mondays and Fridays"),
        bit(1) | bit(5)
    );
}

#[test]
fn reject_bad_weekday_repeats() {
    assert!(parse("Reminder in 30m every Saturday").is_err());
    assert!(parse("Reminder at 7:00AM every Mon-").is_err());
}

#[test]
fn parse_compact_and_math_durations() {
    let r = parse("butterfly unlock in 3d19h17m").unwrap();
    assert_eq!(
        (r.title.as_str(), r.duration_seconds),
        ("butterfly unlock", ((3 * 24 + 19) * 60 + 17) * 60)
    );
    assert_eq!(
        parser::parse_duration_to_seconds("(3*10) minutes"),
        Ok(30 * 60)
    );
    assert_eq!(
        parse("Drink water in (3*10) minutes")
            .unwrap()
            .duration_seconds,
        30 * 60
    );
    assert_eq!(
        parse("Hydrate in 10m every (2+1) hours")
            .unwrap()
            .repeat_interval_seconds,
        3 * 3600
    );
    assert_eq!(
        parser::parse_duration_to_seconds("2+3*4 minutes"),
        Ok(14 * 60)
    );
    assert_eq!(
        parser::parse_duration_to_seconds("(2+3)*4 minutes"),
        Ok(20 * 60)
    );
    assert_eq!(
        parser::parse_duration_to_seconds("2 days 3 hours"),
        Ok(2 * 86400 + 3 * 3600)
    );
    assert_eq!(
        parser::parse_duration_to_seconds("1 hour and 5 mins"),
        Ok(3900)
    );
}

#[test]
fn reject_bad_durations() {
    for bad in [
        "(10/0) minutes",
        "(5/2) minutes",
        "2147483648 seconds",
        "soon",
    ] {
        assert!(
            !parser::parse_duration_to_seconds(bad)
                .unwrap_err()
                .is_empty(),
            "{bad}"
        );
    }
    assert!(parser::parse_repeat_interval_to_seconds("30 seconds").is_err());
}

#[test]
fn weekday_mask_spec_mixed_separators() {
    assert_eq!(
        weekday::parse_weekday_mask_spec("Mon, Wed/Fri+Sun"),
        Ok(bit(1) | bit(3) | bit(5) | bit(7))
    );
}

#[test]
fn checklist_reset() {
    let mut r = Reminder {
        repeating: true,
        checklist: vec![
            ChecklistItem {
                text: "First".into(),
                checked: true,
            },
            ChecklistItem {
                text: "Second".into(),
                checked: false,
            },
            ChecklistItem {
                text: "Third".into(),
                checked: true,
            },
        ],
        ..Default::default()
    };
    r.reset_checklist();
    assert_eq!(r.checked_count(), 0);
    r.repeating = false;
    r.enforce_checklist_constraints();
    assert!(r.checklist.is_empty());
}

fn sample(
    id: &str,
    title: &str,
    repeating: bool,
    kind: ScheduleType,
    secs_from_now: i64,
) -> Reminder {
    Reminder {
        id: id.into(),
        title: title.into(),
        repeating,
        schedule_type: kind,
        next: Local::now() + Duration::seconds(secs_from_now),
        ..Default::default()
    }
}

fn find<'a>(store: &'a ReminderStore, title: &str) -> &'a Reminder {
    store
        .items
        .iter()
        .find(|r| r.title == title)
        .unwrap_or_else(|| panic!("missing {title}"))
}

#[test]
fn share_export_import_round_trip() {
    let mut source = ReminderStore::new();
    let mut repeating = sample(
        "source-repeating-id",
        "Daily Cleanup",
        true,
        ScheduleType::Relative,
        3600,
    );
    repeating.interval_seconds = 86400;
    repeating.checklist = vec![
        ChecklistItem {
            text: "Desk".into(),
            checked: true,
        },
        ChecklistItem {
            text: "Inbox".into(),
            checked: false,
        },
    ];
    let mut one_shot = sample(
        "source-oneshot-id",
        "Doctor Appointment",
        false,
        ScheduleType::AtTimeOfDay,
        86400,
    );
    one_shot.time_of_day = t(14, 30);
    one_shot.checklist = vec![ChecklistItem {
        text: "Should clear on import".into(),
        checked: true,
    }];
    let mut weekly = sample(
        "source-weekly-id",
        "Plan Sprint",
        true,
        ScheduleType::AtTimeOfDay,
        86400,
    );
    weekly.time_of_day = t(8, 15);
    weekly.repeat_weekdays_mask = bit(1) | bit(3) | bit(5);
    source.items = vec![repeating, one_shot, weekly];

    let share = source.export_share_string();
    assert!(share.starts_with("RM1:"));

    let mut imported = ReminderStore::new();
    assert_eq!(imported.import_share_string(&share), Ok(3));
    let r = find(&imported, "Daily Cleanup");
    assert!(r.repeating && r.checklist.len() == 2 && r.checked_count() == 1);
    assert_ne!(r.id, "source-repeating-id");
    let o = find(&imported, "Doctor Appointment");
    assert!(!o.repeating && o.checklist.is_empty());
    let w = find(&imported, "Plan Sprint");
    assert!(w.repeating && w.schedule_type == ScheduleType::AtTimeOfDay);
    assert_eq!(w.repeat_weekdays_mask, bit(1) | bit(3) | bit(5));
    assert_eq!(w.time_of_day, t(8, 15));

    assert!(imported.import_share_string("bad-share-string").is_err());
}

#[test]
fn store_save_load_round_trip() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("documents/RemindMe/reminders.json");
    let mut source = ReminderStore::with_paths(&path, None);
    let mut repeating = sample(
        "repeat-id",
        "Daily Standup",
        true,
        ScheduleType::Relative,
        300,
    );
    repeating.interval_seconds = 3600;
    repeating.checklist = vec![
        ChecklistItem {
            text: "Ping team".into(),
            checked: true,
        },
        ChecklistItem {
            text: "Update notes".into(),
            checked: false,
        },
    ];
    let mut one_shot = sample(
        "oneshot-id",
        "Book dentist",
        false,
        ScheduleType::AtTimeOfDay,
        86400,
    );
    one_shot.time_of_day = t(9, 45);
    let mut weekly = sample(
        "weekly-id",
        "Weekly Sync",
        true,
        ScheduleType::AtTimeOfDay,
        86400,
    );
    weekly.time_of_day = t(11, 0);
    weekly.repeat_weekdays_mask = bit(2) | bit(4);
    source.items = vec![repeating, one_shot, weekly];
    source.completed.push(CompletedReminder {
        id: "completed-id".into(),
        title: "Quick stretch".into(),
        schedule_type: ScheduleType::Relative,
        interval_seconds: 1200,
        time_of_day: None,
        repeat_weekdays_mask: 0,
        completed_at: Local::now(),
        completion_count: 3,
    });
    source.save().unwrap();

    let mut loaded = ReminderStore::with_paths(&path, None);
    loaded.load().unwrap();
    let r = find(&loaded, "Daily Standup");
    assert!(r.repeating && r.checklist.len() == 2);
    assert_eq!(r.next.timestamp(), source.items[0].next.timestamp());
    assert!(find(&loaded, "Book dentist").checklist.is_empty());
    assert_eq!(find(&loaded, "Book dentist").time_of_day, t(9, 45));
    assert_eq!(
        find(&loaded, "Weekly Sync").repeat_weekdays_mask,
        bit(2) | bit(4)
    );
    assert_eq!(loaded.completed.len(), 1);
    assert_eq!(loaded.completed[0].completion_count, 3);
}

#[test]
fn store_migrates_legacy_path() {
    let dir = tempfile::tempdir().unwrap();
    let legacy = dir.path().join("legacy/reminders.json");
    let new = dir.path().join("documents/RemindMe/reminders.json");
    let mut old = ReminderStore::with_paths(&legacy, None);
    old.items.push(sample(
        "legacy-id",
        "Legacy Reminder",
        false,
        ScheduleType::Relative,
        900,
    ));
    old.save().unwrap();

    let mut migrated = ReminderStore::with_paths(&new, Some(legacy));
    migrated.load().unwrap();
    assert!(new.exists());
    find(&migrated, "Legacy Reminder");
}

#[test]
fn store_sort_soonest_first() {
    let mut store = ReminderStore::new();
    store.items = vec![
        sample("third", "Third", false, ScheduleType::Relative, 300),
        sample("first", "First", false, ScheduleType::Relative, 30),
        sample("second", "Second", false, ScheduleType::Relative, 120),
    ];
    store.sort_soonest_first();
    let ids: Vec<_> = store.items.iter().map(|r| r.id.as_str()).collect();
    assert_eq!(ids, ["first", "second", "third"]);
}

fn write_store(json: &str) -> (tempfile::TempDir, ReminderStore) {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("documents/RemindMe/reminders.json");
    std::fs::create_dir_all(path.parent().unwrap()).unwrap();
    std::fs::write(&path, json).unwrap();
    let store = ReminderStore::with_paths(&path, None);
    (dir, store)
}

#[test]
fn store_load_skips_invalid_entries() {
    let now = Local::now().timestamp();
    let (_dir, mut store) = write_store(&format!(
        r#"{{"version":4,"reminders":[
        {{"id":"valid-id","title":"Valid Reminder","next_epoch":{now},"repeating":false,"schedule":"relative","interval_seconds":60,"time_of_day":"","repeat_weekdays_mask":0}},
        {{"id":"invalid-id","title":"","next_epoch":0,"repeating":false,"schedule":"relative","interval_seconds":60,"time_of_day":"","repeat_weekdays_mask":0}}
        ],"completed":[]}}"#
    ));
    store.load().unwrap();
    assert_eq!(store.items.len(), 1);
    assert_eq!(
        (store.items[0].id.as_str(), store.items[0].title.as_str()),
        ("valid-id", "Valid Reminder")
    );
}

#[test]
fn store_missing_file_is_first_run_and_corrupt_fails() {
    let dir = tempfile::tempdir().unwrap();
    let mut store = ReminderStore::with_paths(dir.path().join("x/reminders.json"), None);
    store.load().unwrap();
    assert!(store.items.is_empty() && store.completed.is_empty());

    let (_dir, mut corrupt) = write_store("{not valid json");
    assert!(!corrupt.load().unwrap_err().is_empty());
}

#[test]
fn store_load_caps_completed_history() {
    let now = Local::now().timestamp();
    let completed: Vec<_> = (0..55)
        .map(|i| {
            json!({"id": format!("done-{i}"), "title": format!("Done {i}"), "schedule": "relative",
                   "interval_seconds": 60, "time_of_day": "", "repeat_weekdays_mask": 0,
                   "completed_epoch": (now + i) as f64, "completion_count": i + 1})
        })
        .collect();
    let (_dir, mut store) =
        write_store(&json!({"version": 4, "reminders": [], "completed": completed}).to_string());
    store.load().unwrap();
    assert_eq!(store.completed.len(), 50);
    assert_eq!(store.completed.first().unwrap().id, "done-5");
    assert_eq!(store.completed.last().unwrap().id, "done-54");
}

/// A file exactly as 1.4.1 (Qt) writes it: indented, sorted keys, epochs as numbers.
#[test]
fn store_reads_qt_written_file() {
    let (_dir, mut store) = write_store(
        r#"{
    "completed": [
        {
            "completed_epoch": 1761300000,
            "completion_count": 2,
            "id": "6f1c1f2e-1111-4a5b-9c3d-000000000001",
            "interval_seconds": 0,
            "repeat_weekdays_mask": 0,
            "schedule": "at_time",
            "time_of_day": "06:30",
            "title": "Reinforce"
        }
    ],
    "reminders": [
        {
            "checklist_items": [
                {
                    "checked": true,
                    "text": "Desk"
                }
            ],
            "id": "6f1c1f2e-1111-4a5b-9c3d-000000000002",
            "interval_seconds": 0,
            "next_epoch": 4102444800,
            "repeat_weekdays_mask": 31,
            "repeating": true,
            "schedule": "at_time",
            "time_of_day": "08:15",
            "title": "Plan sprint"
        }
    ],
    "version": 4
}
"#,
    );
    store.load().unwrap();
    let r = &store.items[0];
    assert_eq!(r.next.timestamp(), 4102444800);
    assert_eq!(
        (r.time_of_day, r.repeat_weekdays_mask, r.repeating),
        (t(8, 15), 31, true)
    );
    assert_eq!(
        r.checklist,
        vec![ChecklistItem {
            text: "Desk".into(),
            checked: true
        }]
    );
    let c = &store.completed[0];
    assert_eq!(
        (c.schedule_type, c.time_of_day, c.completion_count),
        (ScheduleType::AtTimeOfDay, t(6, 30), 2)
    );
}

/// A share string produced by 1.4.1 (`QByteArray::toBase64(Base64UrlEncoding | OmitTrailingEquals)`).
#[test]
fn import_qt_share_string() {
    use base64::Engine;
    let raw = r#"{"format":"remindme_share","reminders":[{"checklist_items":[],"id":"x","interval_seconds":2700,"next_epoch":4102444800,"repeat_weekdays_mask":0,"repeating":false,"schedule":"relative","time_of_day":"","title":"Drink water"}],"version":1}"#;
    let share = format!(
        "RM1:{}",
        base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(raw)
    );
    let mut store = ReminderStore::new();
    assert_eq!(store.import_share_string(&share), Ok(1));
    assert_eq!(store.items[0].interval_seconds, 2700);
    // Padded input is accepted too.
    let padded = format!(
        "RM1:{}",
        base64::engine::general_purpose::URL_SAFE.encode(raw)
    );
    assert_eq!(store.import_share_string(&padded), Ok(1));
}

#[test]
fn import_sorts_and_assigns_fresh_ids() {
    let now = Local::now().timestamp();
    let raw = json!({"format": "remindme_share", "version": 1, "reminders": [
        {"id": "incoming-id", "title": "Later", "next_epoch": now + 600, "repeating": false, "schedule": "relative", "interval_seconds": 60},
        {"title": "Sooner", "next_epoch": now + 60, "repeating": false, "schedule": "relative", "interval_seconds": 60}
    ]})
    .to_string();
    let mut store = ReminderStore::new();
    assert_eq!(store.import_share_string(&raw), Ok(2));
    assert_eq!(store.items[0].title, "Sooner");
    assert_eq!(store.items[1].title, "Later");
    assert!(!store.items[1].id.is_empty() && store.items[1].id != "incoming-id");
}

#[test]
fn update_version_comparison() {
    assert!(update::is_remote_version_newer("v1.2.2", "1.2.1"));
    assert!(update::is_remote_version_newer("1.3.0", "1.2.9"));
    assert!(update::is_remote_version_newer(" 2.0.0 ", "1.9.9"));
    assert!(!update::is_remote_version_newer("1.2.1", "1.2.1"));
    assert!(!update::is_remote_version_newer("1.2", "1.1.0"));
    assert!(!update::is_remote_version_newer("v1.2.1", "bad"));
}

fn release_assets() -> Vec<serde_json::Value> {
    vec![
        json!({"name": "RemindMe-2.0.0-windows-portable.zip", "browser_download_url": "https://example.com/p.zip"}),
        json!({"name": "RemindMe-2.0.0-installer.msi", "browser_download_url": "https://example.com/i.msi"}),
        json!({"name": "RemindMe-2.0.0-setup.exe", "browser_download_url": "https://example.com/s.exe", "digest": "sha256:ABCDEF0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789"}),
        json!({"name": "RemindMe-2.0.0-x86_64.AppImage", "browser_download_url": "https://example.com/r.AppImage", "digest": "sha256:feedfeed0123456789abcdeffeedfeed0123456789abcdeffeedfeed01234567"}),
    ]
}

#[test]
fn pick_best_release_asset_per_platform() {
    let win = update::pick_best_release_asset(&release_assets(), Platform::Windows).unwrap();
    assert_eq!(
        (win.name.as_str(), win.sha256_hex.as_str(), win.is_installer),
        (
            "RemindMe-2.0.0-setup.exe",
            "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789",
            true
        )
    );
    let linux = update::pick_best_release_asset(&release_assets(), Platform::Linux).unwrap();
    assert_eq!(
        (
            linux.name.as_str(),
            linux.sha256_hex.as_str(),
            linux.is_installer
        ),
        (
            "RemindMe-2.0.0-x86_64.AppImage",
            "feedfeed0123456789abcdeffeedfeed0123456789abcdeffeedfeed01234567",
            true
        )
    );
    // 1.x clients rank by extension only; the AppImage must never outrank or tie their picks.
    assert_eq!(
        update::update_asset_priority("RemindMe-2.0.0-x86_64.AppImage", Platform::Windows),
        0
    );
}

#[test]
fn pick_best_release_asset_handles_missing_fields() {
    let assets = vec![
        json!("not-an-object"),
        json!({"name": "", "browser_download_url": "https://example.com/no-name.zip"}),
        json!({"name": "RemindMe-1.2.2-setup.exe", "browser_download_url": ""}),
    ];
    assert_eq!(
        update::pick_best_release_asset(&assets, Platform::Windows),
        None
    );
}

#[test]
fn scheduling_rules() {
    let now = Local.with_ymd_and_hms(2030, 6, 12, 10, 0, 0).unwrap(); // a Wednesday
    // Later today, then tomorrow once it has passed.
    assert_eq!(
        schedule::next_at_time_from(t(11, 0).unwrap(), now, 0),
        Local.with_ymd_and_hms(2030, 6, 12, 11, 0, 0).unwrap()
    );
    assert_eq!(
        schedule::next_at_time_from(t(9, 0).unwrap(), now, 0),
        Local.with_ymd_and_hms(2030, 6, 13, 9, 0, 0).unwrap()
    );
    // Weekends only: Saturday.
    assert_eq!(
        schedule::next_at_time_from(t(9, 0).unwrap(), now, weekday::WEEKENDS_MASK),
        Local.with_ymd_and_hms(2030, 6, 15, 9, 0, 0).unwrap()
    );

    // Relative repeats skip whole missed periods and land strictly in the future.
    let mut r = sample("id", "Hydrate", true, ScheduleType::Relative, 0);
    r.interval_seconds = 600;
    r.next = now - Duration::seconds(1500);
    schedule::reschedule_after_acknowledge(&mut r, now);
    assert_eq!(r.next, now + Duration::seconds(300));

    // "at 7am every day" is a daily at-time reminder; "every 2 hours" stays relative.
    let daily = parser::parse_input_at("Stand up at 7:00AM every day", now).unwrap();
    let daily = schedule::reminder_from_parse(&daily, now);
    assert_eq!(
        (daily.schedule_type, daily.repeating),
        (ScheduleType::AtTimeOfDay, true)
    );
    assert_eq!(
        daily.next,
        Local.with_ymd_and_hms(2030, 6, 13, 7, 0, 0).unwrap()
    );

    // History bumps the count for a repeated pattern instead of adding a row.
    let mut history = Vec::new();
    let mut once = sample("a", "Stretch", false, ScheduleType::Relative, 0);
    once.interval_seconds = 1200;
    schedule::append_completed(&mut history, &once, now);
    schedule::append_completed(&mut history, &once, now);
    assert_eq!(history.len(), 1);
    assert_eq!(history[0].completion_count, 2);
    assert_eq!(
        schedule::completed_pattern_text(&history[0]),
        "in 20 minutes"
    );
}

#[test]
fn display_text() {
    use remindme::time_format::*;
    assert_eq!(format_countdown(18 * 60 + 30), "18:30");
    assert_eq!(format_countdown(2 * 3600 + 3 * 60 + 48), "2:03:48");
    assert_eq!(
        format_countdown(3 * 86400 + 20 * 3600 + 49 * 60 + 59),
        "3d 20:49:59"
    );
    assert_eq!(format_interval_text(7200), "2 hours");
    assert_eq!(format_interval_text(60), "1 minute");
    assert_eq!(
        schedule::duration_to_input_text(3 * 86400 + 90),
        "3d 1m 30s"
    );
    assert_eq!(weekday::mask_display_text(bit(1) | bit(3)), "Mon, Wed");
    let r = Reminder {
        repeating: true,
        schedule_type: ScheduleType::AtTimeOfDay,
        repeat_weekdays_mask: weekday::WEEKDAYS_MASK,
        ..Default::default()
    };
    assert_eq!(schedule::repeats_cell_text(&r), "Weekdays");
}

#[test]
fn greetings_file_round_trip() {
    use remindme::greetings;
    let pools = greetings::defaults();
    assert_eq!(greetings::parse(&greetings::render(&pools)), pools);
    let custom = greetings::parse("[evening]\nNight!\n");
    assert_eq!(custom[2], vec!["Night!".to_string()]);
    assert_eq!(custom[0], pools[0]);
    // The repo's sample greetings.txt fills all three sections.
    let shipped =
        std::fs::read_to_string(concat!(env!("CARGO_MANIFEST_DIR"), "/greetings.txt")).unwrap();
    assert!(greetings::parse(&shipped).iter().all(|p| p.len() == 10));
}

#[test]
fn reschedule_after_acknowledge_at_time() {
    let now = Local.with_ymd_and_hms(2030, 6, 12, 7, 0, 0).unwrap();
    // Daily at-time: strictly after `now`, so due-exactly-now moves to tomorrow.
    let mut daily = sample("d", "Stand up", true, ScheduleType::AtTimeOfDay, 0);
    daily.time_of_day = t(7, 0);
    daily.next = now;
    schedule::reschedule_after_acknowledge(&mut daily, now);
    assert_eq!(
        daily.next,
        Local.with_ymd_and_hms(2030, 6, 13, 7, 0, 0).unwrap()
    );

    // Weekday mask: Friday 10:00 -> next selected day is Monday.
    let friday = Local.with_ymd_and_hms(2030, 6, 14, 10, 0, 0).unwrap();
    let mut weekdays = sample("w", "Standup", true, ScheduleType::AtTimeOfDay, 0);
    weekdays.time_of_day = t(9, 0);
    weekdays.repeat_weekdays_mask = weekday::WEEKDAYS_MASK;
    weekdays.next = friday - Duration::hours(1);
    schedule::reschedule_after_acknowledge(&mut weekdays, friday);
    assert_eq!(
        weekdays.next,
        Local.with_ymd_and_hms(2030, 6, 17, 9, 0, 0).unwrap()
    );
}

#[test]
fn append_completed_caps_history() {
    let now = fixed_now();
    let mut history = Vec::new();
    for i in 0..MAX_COMPLETED_ITEMS + 5 {
        let mut r = sample("id", &format!("Task {i}"), false, ScheduleType::Relative, 0);
        r.interval_seconds = 60;
        schedule::append_completed(&mut history, &r, now);
    }
    assert_eq!(history.len(), MAX_COMPLETED_ITEMS);
    assert_eq!(history.first().unwrap().title, "Task 5");
    assert_eq!(
        history.last().unwrap().title,
        format!("Task {}", MAX_COMPLETED_ITEMS + 4)
    );
}
