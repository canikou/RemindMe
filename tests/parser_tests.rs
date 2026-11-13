// SPDX-License-Identifier: MIT
//! Parser edge cases and the scheduling rules built on them, all against a fixed `now`.

use chrono::{DateTime, Local, NaiveDate, NaiveTime, TimeZone};
use remindme::model::ScheduleType;
use remindme::parser::{self, ParseResult};
use remindme::schedule;

fn at(y: i32, mo: u32, d: u32, h: u32, mi: u32) -> DateTime<Local> {
    Local.with_ymd_and_hms(y, mo, d, h, mi, 0).unwrap()
}

fn parse(input: &str, now: DateTime<Local>) -> Result<ParseResult, String> {
    parser::parse_input_at(input, now)
}

fn date(y: i32, m: u32, d: u32) -> Option<NaiveDate> {
    NaiveDate::from_ymd_opt(y, m, d)
}

fn time(h: u32, m: u32) -> Option<NaiveTime> {
    NaiveTime::from_hms_opt(h, m, 0)
}

#[test]
fn decimal_and_dangling_durations_are_errors() {
    let now = at(2030, 6, 12, 10, 0);
    for bad in ["Stretch in 1.5h", "Stretch in 0.5 hours", "Call in 1h 30"] {
        assert!(parse(bad, now).is_err(), "{bad}");
    }
    assert!(parser::parse_duration_to_seconds("1.5h").is_err());
    // Whole-number forms keep working.
    assert_eq!(parser::parse_duration_to_seconds("1h 30m"), Ok(5400));
    assert_eq!(parser::parse_duration_to_seconds("1h, 30m"), Ok(5400));
}

#[test]
fn non_ascii_digits_do_not_parse_as_zero() {
    let now = at(2030, 6, 12, 10, 0);
    assert!(parse("Stand up at \u{FF17}:30", now).is_err());
    assert!(parse("Stand up at 7:\u{FF13}0", now).is_err());
    assert!(parser::parse_time_of_day("\u{FF17}:30").is_err());
    assert!(parse("Stand up in \u{FF15}m", now).is_err());
}

#[test]
fn math_overflow_is_an_error_not_a_panic() {
    let now = at(2030, 6, 12, 10, 0);
    let expr = "(-9223372036854775807-1)/-1 m";
    let e = parser::parse_duration_to_seconds(expr).unwrap_err();
    assert!(e.contains("overflowed"), "{e}");
    assert!(parse(&format!("x in {expr}"), now).is_err());
}

#[test]
fn deeply_nested_math_is_an_error() {
    let parens = format!("{}1{} m", "(".repeat(20_000), ")".repeat(20_000));
    let e = parser::parse_duration_to_seconds(&parens).unwrap_err();
    assert!(e.contains("nested"), "{e}");
    let negs = format!("{}1 m", "-".repeat(20_000));
    let e = parser::parse_duration_to_seconds(&negs).unwrap_err();
    assert!(e.contains("nested"), "{e}");
    // Ordinary nesting is fine.
    assert_eq!(parser::parse_duration_to_seconds("((((2)))) m"), Ok(120));
}

#[test]
fn feb_29_without_a_year_finds_the_next_leap_year() {
    let r = parse("Dentist on Feb 29 9am", at(2026, 3, 1, 10, 0)).unwrap();
    assert_eq!((r.date, r.time_of_day), (date(2028, 2, 29), time(9, 0)));
    // The clamp to Feb 28 of the following year used to win here.
    let r = parse("Dentist on Feb 29 9am", at(2028, 3, 1, 10, 0)).unwrap();
    assert_eq!(r.date, date(2032, 2, 29));
    // Still ahead this leap year.
    let r = parse("Dentist on Feb 29 9am", at(2028, 1, 10, 10, 0)).unwrap();
    assert_eq!(r.date, date(2028, 2, 29));
    // A date that never exists stays an error.
    let e = parse("x on Feb 30 9am", at(2026, 3, 1, 10, 0)).unwrap_err();
    assert!(e.contains("doesn't exist"), "{e}");
}

#[test]
fn weekday_rolls_a_week_once_the_time_has_passed() {
    let sunday_after_8 = at(2030, 6, 16, 10, 0);
    let r = parse("Vacuum on Sunday 8am", sunday_after_8).unwrap();
    assert_eq!((r.date, r.time_of_day), (date(2030, 6, 23), time(8, 0)));
    // Still ahead today: no roll.
    let sunday_early = at(2030, 6, 16, 7, 0);
    let r = parse("Vacuum on Sunday 8am", sunday_early).unwrap();
    assert_eq!(r.date, date(2030, 6, 16));
}

#[test]
fn year_rolls_over_for_dates_without_a_year() {
    let r = parse("Party on Jan 1 9am", at(2030, 12, 31, 10, 0)).unwrap();
    assert_eq!(r.date, date(2031, 1, 1));
}

#[test]
fn bare_tomorrow_defaults_to_nine_am() {
    let r = parse("Water plants tomorrow", at(2030, 12, 31, 22, 0)).unwrap();
    assert_eq!(
        (r.title.as_str(), r.date, r.time_of_day),
        ("Water plants", date(2031, 1, 1), time(9, 0))
    );
}

#[test]
fn at_time_with_interval_first_fires_at_the_given_time() {
    let now = at(2030, 6, 12, 10, 0);
    let p = parse("Pills at 7am every 2 hours", now).unwrap();
    let r = schedule::reminder_from_parse(&p, now);
    assert_eq!(r.schedule_type, ScheduleType::Relative);
    assert_eq!((r.repeating, r.interval_seconds), (true, 7200));
    assert_eq!(r.next, at(2030, 6, 13, 7, 0));

    // 7am still ahead today.
    let early = at(2030, 6, 12, 5, 0);
    let p = parse("Pills at 7am every 2 hours", early).unwrap();
    assert_eq!(
        schedule::reminder_from_parse(&p, early).next,
        at(2030, 6, 12, 7, 0)
    );

    let p = parse("Water at 9am every 2 days", now).unwrap();
    let r = schedule::reminder_from_parse(&p, now);
    assert_eq!(r.interval_seconds, 2 * 86400);
    assert_eq!(r.next, at(2030, 6, 13, 9, 0));

    // After Done it continues by the interval from that first fire.
    let mut p =
        schedule::reminder_from_parse(&parse("Pills at 7am every 2 hours", now).unwrap(), now);
    schedule::reschedule_after_acknowledge(&mut p, at(2030, 6, 13, 7, 0));
    assert_eq!(p.next, at(2030, 6, 13, 9, 0));

    // The live preview describes the same first fire.
    let text: String = schedule::input_preview("Pills at 7am every 2 hours", now)
        .into_iter()
        .map(|(t, _)| t)
        .collect();
    assert!(text.contains("first tomorrow at 7:00 AM"), "{text}");
    assert!(text.contains("repeats every 2 hours"), "{text}");
}
