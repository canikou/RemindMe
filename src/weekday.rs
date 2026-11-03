// SPDX-License-Identifier: MIT
//! Weekday masks. Day numbers follow ISO / Qt: 1 = Monday .. 7 = Sunday; bit 0 = Monday.

pub const ALL_MASK: i32 = 0x7F;
pub const WEEKDAYS_MASK: i32 = 0b0011111;
pub const WEEKENDS_MASK: i32 = 0b1100000;

pub fn weekday_bit(day: i32) -> i32 {
    if (1..=7).contains(&day) {
        1 << (day - 1)
    } else {
        0
    }
}

pub fn normalize_mask(raw: i32) -> i32 {
    raw & ALL_MASK
}

/// An empty mask selects every day.
pub fn is_selected(mask: i32, day: i32) -> bool {
    let mask = normalize_mask(mask);
    mask == 0 || mask & weekday_bit(day) != 0
}

pub fn weekday_from_token(token: &str) -> i32 {
    let mut t = token.trim().to_lowercase();
    while t.ends_with('.') || t.ends_with(',') {
        t.pop();
    }
    if t.ends_with('s') && t.chars().count() > 3 {
        t.pop();
    }
    match t.as_str() {
        "mon" | "monday" => 1,
        "tue" | "tues" | "tuesday" => 2,
        "wed" | "weds" | "wednesday" => 3,
        "thu" | "thur" | "thurs" | "thursday" => 4,
        "fri" | "friday" => 5,
        "sat" | "saturday" => 6,
        "sun" | "sunday" => 7,
        _ => 0,
    }
}

const SHORT_NAMES: [&str; 7] = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];

pub fn mask_display_text(mask: i32) -> String {
    let mask = normalize_mask(mask);
    if mask == 0 || mask == ALL_MASK {
        return "daily".into();
    }
    if mask == WEEKDAYS_MASK {
        return "weekdays".into();
    }
    if mask == WEEKENDS_MASK {
        return "weekends".into();
    }
    (1..=7)
        .filter(|d| mask & weekday_bit(*d) != 0)
        .map(|d| SHORT_NAMES[(d - 1) as usize])
        .collect::<Vec<_>>()
        .join(", ")
}

fn range_mask_inclusive(start: i32, end: i32) -> i32 {
    if !(1..=7).contains(&start) || !(1..=7).contains(&end) {
        return 0;
    }
    let mut mask = 0;
    let mut day = start;
    for _ in 0..7 {
        mask |= weekday_bit(day);
        if day == end {
            break;
        }
        day = if day == 7 { 1 } else { day + 1 };
    }
    mask
}

fn is_range_connector(t: &str) -> bool {
    matches!(t, "-" | "to" | "through" | "thru")
}

pub fn parse_weekday_mask_spec(spec: &str) -> Result<i32, String> {
    let mut normalized = spec.to_lowercase();
    for dash in ['\u{2013}', '\u{2014}', '\u{2212}'] {
        normalized = normalized.replace(dash, "-");
    }
    for sep in [',', '/', '&', '+', '.'] {
        normalized = normalized.replace(sep, " ");
    }
    let tokens: Vec<&str> = normalized.split_whitespace().collect();
    if tokens.is_empty() {
        return Err("Repeat weekdays are empty.".into());
    }

    let mut mask = 0;
    let mut i = 0;
    while i < tokens.len() {
        let token = tokens[i];
        i += 1;
        if matches!(
            token,
            "on" | "and" | "each" | "every" | "the" | "etc" | "from"
        ) {
            continue;
        }
        if token == "weekday" || token == "weekdays" {
            mask |= WEEKDAYS_MASK;
            continue;
        }
        if token == "weekend" || token == "weekends" {
            mask |= WEEKENDS_MASK;
            continue;
        }
        if let Some(pos) = token.find('-')
            && pos > 0
            && pos < token.len() - 1
        {
            let (start, end) = (
                weekday_from_token(&token[..pos]),
                weekday_from_token(&token[pos + 1..]),
            );
            if start == 0 || end == 0 {
                return Err(format!("Unrecognized weekday range \"{token}\"."));
            }
            mask |= range_mask_inclusive(start, end);
            continue;
        }
        let day = weekday_from_token(token);
        if day == 0 {
            return Err(format!(
                "Unrecognized weekday \"{token}\". Use names like Monday, Wed, or Fri."
            ));
        }
        if i + 1 < tokens.len() && is_range_connector(tokens[i]) {
            let end = weekday_from_token(tokens[i + 1]);
            if end == 0 {
                return Err(format!(
                    "Unrecognized weekday \"{}\" in range.",
                    tokens[i + 1]
                ));
            }
            mask |= range_mask_inclusive(day, end);
            i += 2;
            continue;
        }
        mask |= weekday_bit(day);
    }

    match normalize_mask(mask) {
        0 => Err("Repeat weekdays are empty.".into()),
        m => Ok(m),
    }
}
