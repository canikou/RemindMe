// SPDX-License-Identifier: MIT
//! Natural-language reminder input: "Drink water in 45m", "Stand up at 7:00AM every weekday",
//! "Dentist on Oct 24 6:30AM". Behavior mirrors the 1.x C++ parser.

use crate::model::local;
use crate::weekday;
use chrono::{DateTime, Datelike, Duration, Local, Months, NaiveDate, NaiveTime};
use fancy_regex::Regex;
use std::sync::LazyLock;

#[derive(Clone, Debug, Default, PartialEq)]
pub struct ParseResult {
    pub title: String,
    /// true = "in", false = "at"/"on".
    pub is_relative: bool,
    pub duration_seconds: i32,
    pub time_of_day: Option<NaiveTime>,
    /// Set for "on <date>".
    pub date: Option<NaiveDate>,
    pub has_repeat_directive: bool,
    pub repeat_interval_seconds: i32,
    pub repeat_weekdays_mask: i32,
}

macro_rules! re {
    ($name:ident, $pat:expr) => {
        static $name: LazyLock<Regex> = LazyLock::new(|| Regex::new($pat).expect("valid regex"));
    };
}

const MONTHS: &str = "jan(?:uary)?|feb(?:ruary)?|mar(?:ch)?|apr(?:il)?|may|june?|july?|aug(?:ust)?|sep(?:t(?:ember)?)?|oct(?:ober)?|nov(?:ember)?|dec(?:ember)?";
const DAY_WORDS: &str = "monday|mon|tuesday|tues|tue|wednesday|weds|wed|thursday|thurs|thur|thu|friday|fri|saturday|sat|sunday|sun";

re!(SECOND_UNITS, r"(?i)\b(s|sec|secs|second|seconds)\b");
re!(
    WEEKDAY_HINT,
    r"(?i)\b(mon(day)?s?|tue(s|sday)?s?|wed(nesday)?s?|thu(r|rs|rsday)?s?|fri(day)?s?|sat(urday)?s?|sun(day)?s?|weekday(s)?|weekend(s)?)\b"
);
re!(REPEAT_DIRECTIVE, r"(?i)^(.*)\bevery\b\s+(.+)$");
re!(SCHEDULE_KEYWORD, r"(?i)\b(in|at|on)\b");
re!(
    DURATION_TOKEN,
    r"([\d\+\-\*\/\(\)\s]+)\s*(d|day|days|h|hr|hrs|hour|hours|m|min|mins|minute|minutes|s|sec|secs|second|seconds)(?![a-z])"
);
re!(TIME_OF_DAY, r"^(\d{1,2})(?::(\d{2}))?([AaPp][Mm])?$");
re!(
    WEEKDAY,
    &format!(r"(?i)\b(?:next\s+|this\s+)?({DAY_WORDS})\b")
);
re!(
    MONTH_DAY,
    &format!(r"(?i)\b({MONTHS})\.?\s+(\d{{1,2}})(?:st|nd|rd|th)?(?:\s*,?\s*(\d{{4}}))?\b")
);
re!(
    DAY_MONTH,
    &format!(
        r"(?i)\b(\d{{1,2}})(?:st|nd|rd|th)?\s+(?:of\s+)?({MONTHS})\.?(?:\s*,?\s*(\d{{4}}))?\b"
    )
);
re!(ISO_DATE, r"\b(\d{4})-(\d{1,2})-(\d{1,2})\b");
re!(SLASH_DATE, r"\b(\d{1,2})/(\d{1,2})/(\d{4})\b");
re!(RELATIVE_DAY, r"(?i)\b(today|tomorrow)\b");
re!(BARE_TOMORROW, r"(?i)^(.*\S)\s+tomorrow$");
re!(ENDS_WITH_TIME, r"\d(?::\d{2})?\s*([AaPp][Mm])?$");
re!(
    TRAILING_DAY,
    &format!(r"(?i)^(.*?\S)\s+(?:on\s+)?((?:next\s+|this\s+)?(?:today|tomorrow|{DAY_WORDS}))$")
);
re!(TRAILING_ON_DATE, r"(?i)^(.*\S)\s+on\s+(.+)$");
re!(AT_TIME, r"^(.*)\bat\s+([\d:\sAaPpMm]+)$");

fn caps<'a>(re: &Regex, s: &'a str) -> Option<fancy_regex::Captures<'a, str>> {
    re.captures(s).ok().flatten()
}

fn group<'a>(c: &fancy_regex::Captures<'a, str>, i: usize) -> &'a str {
    c.get(i).map_or("", |m| m.as_str())
}

fn to_int(s: &str) -> i32 {
    s.parse().unwrap_or(0)
}

fn month_from_name(name: &str) -> u32 {
    let lower: String = name.chars().take(3).collect::<String>().to_lowercase();
    [
        "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec",
    ]
    .iter()
    .position(|m| *m == lower)
    .map_or(0, |i| i as u32 + 1)
}

struct ExtractedDate {
    year: i32, // 0 = not given
    month: u32,
    day: u32,
    /// Set for "the next such weekday": add these days if that time already passed today.
    roll_days_if_past: i64,
}

fn remove_range(s: &mut String, start: usize, end: usize) {
    s.replace_range(start..end, "");
}

/// Finds a date in `s`, removes it from `s`, and returns it.
fn extract_date(s: &mut String, today: NaiveDate) -> Option<ExtractedDate> {
    let ymd = |d: NaiveDate| (d.year(), d.month(), d.day());
    let (year, month, day, range) = if let Some(c) = caps(&RELATIVE_DAY, s) {
        let add = if group(&c, 1).eq_ignore_ascii_case("tomorrow") {
            1
        } else {
            0
        };
        let (y, m, d) = ymd(today + Duration::days(add));
        let m0 = c.get(0).unwrap();
        (y, m, d, m0.range())
    } else if let Some(c) = caps(&MONTH_DAY, s) {
        let m0 = c.get(0).unwrap();
        (
            to_int(group(&c, 3)),
            month_from_name(group(&c, 1)),
            to_int(group(&c, 2)) as u32,
            m0.range(),
        )
    } else if let Some(c) = caps(&DAY_MONTH, s) {
        let m0 = c.get(0).unwrap();
        (
            to_int(group(&c, 3)),
            month_from_name(group(&c, 2)),
            to_int(group(&c, 1)) as u32,
            m0.range(),
        )
    } else if let Some(c) = caps(&ISO_DATE, s) {
        let m0 = c.get(0).unwrap();
        (
            to_int(group(&c, 1)),
            to_int(group(&c, 2)) as u32,
            to_int(group(&c, 3)) as u32,
            m0.range(),
        )
    } else if let Some(c) = caps(&SLASH_DATE, s) {
        let m0 = c.get(0).unwrap();
        (
            to_int(group(&c, 3)),
            to_int(group(&c, 1)) as u32,
            to_int(group(&c, 2)) as u32,
            m0.range(),
        )
    } else {
        let c = caps(&WEEKDAY, s)?;
        let target = weekday::weekday_from_token(group(&c, 1)) as i64;
        let current = today.weekday().number_from_monday() as i64;
        let (y, m, d) = ymd(today + Duration::days((target - current + 7) % 7));
        let r = c.get(0).unwrap().range();
        remove_range(s, r.start, r.end);
        return Some(ExtractedDate {
            year: y,
            month: m,
            day: d,
            roll_days_if_past: 7,
        });
    };

    remove_range(s, range.start, range.end);
    // "Sunday, Oct 26": the explicit date wins; drop the weekday word so it isn't read as the time.
    *s = WEEKDAY.replace_all(s, "").into_owned();
    Some(ExtractedDate {
        year,
        month,
        day,
        roll_days_if_past: 0,
    })
}

// ---- integer math for durations: + - * / and parentheses, whole numbers only ----

struct IntExpr<'a> {
    chars: Vec<char>,
    pos: usize,
    error: Option<String>,
    _src: &'a str,
}

impl<'a> IntExpr<'a> {
    fn new(src: &'a str) -> Self {
        Self {
            chars: src.chars().collect(),
            pos: 0,
            error: None,
            _src: src,
        }
    }

    fn fail<T>(&mut self, msg: &str) -> Option<T> {
        self.error.get_or_insert_with(|| msg.to_string());
        None
    }

    fn at_end(&self) -> bool {
        self.pos >= self.chars.len()
    }

    fn skip_spaces(&mut self) {
        while !self.at_end() && self.chars[self.pos].is_whitespace() {
            self.pos += 1;
        }
    }

    fn parse(mut self) -> Result<i64, String> {
        let value = self.expression();
        let value = match value {
            Some(v) => v,
            None => {
                return Err(self
                    .error
                    .unwrap_or_else(|| "Invalid math expression.".into()));
            }
        };
        self.skip_spaces();
        if !self.at_end() {
            return Err(format!(
                "Unexpected token in math expression near \"{}\".",
                self.chars[self.pos]
            ));
        }
        Ok(value)
    }

    fn expression(&mut self) -> Option<i64> {
        let mut value = self.term()?;
        loop {
            self.skip_spaces();
            if self.at_end() {
                return Some(value);
            }
            let op = self.chars[self.pos];
            if op != '+' && op != '-' {
                return Some(value);
            }
            self.pos += 1;
            let rhs = self.term()?;
            value = match if op == '+' {
                value.checked_add(rhs)
            } else {
                value.checked_sub(rhs)
            } {
                Some(v) => v,
                None => return self.fail("Math expression overflowed."),
            };
        }
    }

    fn term(&mut self) -> Option<i64> {
        let mut value = self.factor()?;
        loop {
            self.skip_spaces();
            if self.at_end() {
                return Some(value);
            }
            let op = self.chars[self.pos];
            if op != '*' && op != '/' {
                return Some(value);
            }
            self.pos += 1;
            let rhs = self.factor()?;
            value = if op == '*' {
                match value.checked_mul(rhs) {
                    Some(v) => v,
                    None => return self.fail("Math expression overflowed."),
                }
            } else if rhs == 0 {
                return self.fail("Division by zero in math expression.");
            } else if value % rhs != 0 {
                return self.fail("Math expression must resolve to a whole number.");
            } else {
                value / rhs
            };
        }
    }

    fn factor(&mut self) -> Option<i64> {
        self.skip_spaces();
        if self.at_end() {
            return self.fail("Math expression ended unexpectedly.");
        }
        let c = self.chars[self.pos];
        if c == '+' || c == '-' {
            self.pos += 1;
            let v = self.factor()?;
            if c == '-' {
                return match v.checked_neg() {
                    Some(n) => Some(n),
                    None => self.fail("Math expression overflowed."),
                };
            }
            return Some(v);
        }
        if c == '(' {
            self.pos += 1;
            let v = self.expression()?;
            self.skip_spaces();
            if self.at_end() || self.chars[self.pos] != ')' {
                return self.fail("Missing ')' in math expression.");
            }
            self.pos += 1;
            return Some(v);
        }
        self.number()
    }

    fn number(&mut self) -> Option<i64> {
        self.skip_spaces();
        let start = self.pos;
        while !self.at_end() && self.chars[self.pos].is_ascii_digit() {
            self.pos += 1;
        }
        if start == self.pos {
            return self.fail("Expected a number in math expression.");
        }
        let digits: String = self.chars[start..self.pos].iter().collect();
        match digits.parse() {
            Ok(v) => Some(v),
            Err(_) => self.fail("Number in math expression is out of range."),
        }
    }
}

fn parse_duration_amount(raw: &str) -> Result<i64, String> {
    let amount = raw.trim();
    if amount.is_empty() {
        return Err("Duration amount is empty.".into());
    }
    let v = IntExpr::new(amount).parse()?;
    if v <= 0 {
        return Err("Duration amounts must be > 0.".into());
    }
    Ok(v)
}

fn unit_seconds(unit: &str) -> i64 {
    match unit {
        "d" | "day" | "days" => 86400,
        "h" | "hr" | "hrs" | "hour" | "hours" => 3600,
        "m" | "min" | "mins" | "minute" | "minutes" => 60,
        _ => 1,
    }
}

pub fn parse_duration_to_seconds(s: &str) -> Result<i32, String> {
    let x = s.to_lowercase().replace(',', " ").replace("and", " ");
    let mut total: i64 = 0;
    let mut matches = 0;
    for c in DURATION_TOKEN.captures_iter(x.as_str()) {
        let c = c.map_err(|e| e.to_string())?;
        let n = parse_duration_amount(group(&c, 1))?;
        let delta = n
            .checked_mul(unit_seconds(group(&c, 2)))
            .ok_or("Duration is too large.")?;
        total = total.checked_add(delta).ok_or("Duration is too large.")?;
        matches += 1;
    }
    if matches == 0 {
        return Err(
            "Couldn't parse duration. Examples: \"45m\", \"5h23m\", \"2 days 3 hours\"".into(),
        );
    }
    if total <= 0 {
        return Err("Duration must be > 0.".into());
    }
    i32::try_from(total).map_err(|_| "Duration is too large.".into())
}

pub fn parse_time_of_day(s: &str) -> Result<NaiveTime, String> {
    let x: String = s.trim().chars().filter(|c| *c != ' ').collect();
    let Some(c) = caps(&TIME_OF_DAY, &x) else {
        return Err("Couldn't parse time. Examples: \"7:00AM\", \"7am\", \"19:30\"".into());
    };
    let mut hour = to_int(group(&c, 1));
    let minute = to_int(group(&c, 2));
    let ampm = group(&c, 3).to_lowercase();
    if !(0..=59).contains(&minute) {
        return Err("Minutes must be 00-59.".into());
    }
    if !ampm.is_empty() {
        if !(1..=12).contains(&hour) {
            return Err("For AM/PM time, hour must be 1-12.".into());
        }
        if ampm == "am" {
            if hour == 12 {
                hour = 0;
            }
        } else if hour != 12 {
            hour += 12;
        }
    } else if !(0..=23).contains(&hour) {
        return Err("Hour must be 0-23 (or use AM/PM).".into());
    }
    NaiveTime::from_hms_opt(hour as u32, minute as u32, 0).ok_or_else(|| "Invalid time.".into())
}

pub fn parse_date_time(s: &str, now: DateTime<Local>) -> Result<(NaiveDate, NaiveTime), String> {
    let mut rest = s.to_string();
    let Some(d) = extract_date(&mut rest, now.date_naive()) else {
        return Err("Couldn't parse date. Examples: \"on Sunday 8AM\", \"on October 24, 2026 6:30AM\", \"on 2026-10-24 18:30\"".into());
    };
    let rest = rest.replace(',', "");
    let rest = rest.trim();
    if rest.is_empty() {
        return Err(
            "Add a time of day after the date, e.g. \"on October 24, 2026 6:30AM\".".into(),
        );
    }
    let time = parse_time_of_day(rest)?;

    let year_given = d.year != 0;
    let mut date =
        NaiveDate::from_ymd_opt(if year_given { d.year } else { now.year() }, d.month, d.day)
            .ok_or("That date doesn't exist.")?;
    if d.roll_days_if_past > 0 && local(date, time) <= now {
        date += Duration::days(d.roll_days_if_past);
    }
    if !year_given && local(date, time) <= now {
        date = date.checked_add_months(Months::new(12)).unwrap_or(date);
    }
    if local(date, time) <= now {
        return Err("That date and time is already in the past.".into());
    }
    Ok((date, time))
}

pub fn parse_repeat_interval_to_seconds(s: &str) -> Result<i32, String> {
    let x = s.trim().to_lowercase();
    if x.is_empty() {
        return Err("Repeat interval is empty.".into());
    }
    match x.as_str() {
        "day" | "daily" | "everyday" => return Ok(86400),
        "hour" | "hours" | "hr" | "hrs" => return Ok(3600),
        "minute" | "minutes" | "min" | "mins" => return Ok(60),
        _ => {}
    }
    const UNSUPPORTED: &str = "Repeat interval supports minutes, hours, or days.";
    if SECOND_UNITS.is_match(s).unwrap_or(false) {
        return Err(UNSUPPORTED.into());
    }
    let seconds = parse_duration_to_seconds(s)?;
    if seconds < 60 {
        return Err(UNSUPPORTED.into());
    }
    Ok(seconds)
}

fn extract_date_probe(s: &str, today: NaiveDate) -> bool {
    extract_date(&mut s.to_string(), today).is_some()
}

pub fn parse_input(input: &str) -> Result<ParseResult, String> {
    parse_input_at(input, Local::now())
}

pub fn parse_input_at(input: &str, now: DateTime<Local>) -> Result<ParseResult, String> {
    let s = input.trim();
    if s.is_empty() {
        return Err("Type something like: \"Drink water in 45m\" or \"Stand up at 7:00AM\"".into());
    }

    let mut expression = s.to_string();
    let mut repeat_spec = String::new();
    if let Some(c) = caps(&REPEAT_DIRECTIVE, s) {
        expression = group(&c, 1).trim().to_string();
        repeat_spec = group(&c, 2).trim().to_string();
        if expression.is_empty() || repeat_spec.is_empty() {
            return Err(
                "Invalid repeating format. Example: \"Take break at 7:00AM every day\"".into(),
            );
        }
    }

    // "Water plants tomorrow" with no time: schedule tomorrow at 9:00 AM (the preview states it).
    if let Some(c) = caps(&BARE_TOMORROW, &expression) {
        let head = group(&c, 1).to_string();
        if !ENDS_WITH_TIME.is_match(&head).unwrap_or(false) {
            expression = format!("{head} on tomorrow 9:00AM");
        }
    }

    let Some(last) = SCHEDULE_KEYWORD
        .captures_iter(expression.as_str())
        .filter_map(Result::ok)
        .last()
    else {
        return Err(
            "Say when: add \"in 45m\", \"at 7:00AM\" or \"on Oct 24 6:30AM\" after the reminder."
                .into(),
        );
    };
    let whole = last.get(0).unwrap();
    let keyword = group(&last, 1).to_lowercase();
    let mut left = expression[..whole.start()].trim().to_string();
    let mut right = expression[whole.end()..].trim().to_string();
    let today = now.date_naive();

    // "<title> [on] <day> at <time>": fold the day into the right-hand side.
    if keyword == "at" {
        if let Some(c) = caps(&TRAILING_DAY, &left) {
            right = format!("{right} {}", group(&c, 2));
            left = group(&c, 1).trim().to_string();
        } else if let Some(c) = caps(&TRAILING_ON_DATE, &left) {
            // Only fold when everything after "on" is a date, so "Turn on lights at 7pm" keeps its title.
            let mut candidate = group(&c, 2).to_string();
            if extract_date(&mut candidate, today).is_some()
                && candidate.replace(',', "").trim().is_empty()
            {
                right = format!("{right} {}", group(&c, 2));
                left = group(&c, 1).trim().to_string();
            }
        }
    }

    // "<title> at <time> on <date>": fold the time into the right-hand side.
    if keyword == "on"
        && let Some(c) = caps(&AT_TIME, &left)
        && !group(&c, 1).trim().is_empty()
    {
        right = format!("{} {right}", group(&c, 2).trim());
        left = group(&c, 1).trim().to_string();
    }

    if left.is_empty() {
        return Err("Reminder name is empty. Put text before \"in\" or \"at\".".into());
    }
    if right.is_empty() {
        return Err("Time part is empty. Put time after \"in\" or \"at\".".into());
    }

    let mut r = ParseResult {
        title: left,
        ..Default::default()
    };

    if keyword == "in" {
        r.is_relative = true;
        r.duration_seconds = parse_duration_to_seconds(&right)?;
    } else {
        r.is_relative = false;
        if keyword == "on" || extract_date_probe(&right, today) {
            let (date, time) = parse_date_time(&right, now)?;
            if !repeat_spec.is_empty() {
                return Err("Repeat is not supported with a specific date.".into());
            }
            r.date = Some(date);
            r.time_of_day = Some(time);
        } else {
            r.time_of_day = Some(parse_time_of_day(&right)?);
        }
    }

    if !repeat_spec.is_empty() {
        if WEEKDAY_HINT.is_match(&repeat_spec).unwrap_or(false) {
            let mask = weekday::parse_weekday_mask_spec(&repeat_spec)?;
            if r.is_relative {
                return Err("Weekday repeat is only supported with \"at\" reminders.".into());
            }
            r.has_repeat_directive = true;
            r.repeat_weekdays_mask = mask;
        } else {
            r.repeat_interval_seconds = parse_repeat_interval_to_seconds(&repeat_spec)?;
            r.has_repeat_directive = true;
        }
    }
    Ok(r)
}
