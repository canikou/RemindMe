// SPDX-License-Identifier: MIT
//! Editable greeting bank in `Documents/RemindMe/greetings.txt`.

use std::fs;
use std::path::Path;

const MORNING: [&str; 10] = [
    "Good morning, have we had coffee yet? ☕",
    "Good morning! Tiny steps still count 🌤️",
    "Morning! Let's keep it easy and steady 🌱",
    "Good morning! You've got this 💪",
    "Morning reset: breathe, then begin 🌬️",
    "Morning check-in: water first 💧",
    "Morning focus: one thing at a time 🎯",
    "Morning mood: calm progress is still progress 🍃",
    "Morning plan: start small, win early ✨",
    "Morning reminder: be kind to yourself today 🌅",
];
const AFTERNOON: [&str; 10] = [
    "Good afternoon! Hydration check 💧",
    "Afternoon mode: progress over perfection ✨",
    "Good afternoon! One reminder at a time 📌",
    "Afternoon vibes: keep it simple 🌼",
    "Afternoon checkpoint: how is your energy? 🔋",
    "Afternoon reset: quick stretch break? 🤸",
    "Afternoon focus: pick the next tiny step 🎯",
    "Afternoon momentum: progress over perfection 🚀",
    "Afternoon reminder: do not forget to blink 😄",
    "Afternoon vibes: steady pace, steady results 🚶",
];
const EVENING: [&str; 10] = [
    "Good evening! Time to wrap up gently 🌙",
    "Evening check-in: what matters next? 📝",
    "Good evening! You made it through today 🌃",
    "Night mode, calm pace 😌",
    "Good evening! Rest counts too 🛌",
    "Good evening! You did enough for today ✅",
    "Evening reset: light tasks only 🕯️",
    "Evening focus: finish strong, not fast 🧹",
    "Evening reminder: let tomorrow handle tomorrow 🌙",
    "Evening mood: quiet progress still counts 🌌",
];

const SECTIONS: [&str; 3] = ["[morning]", "[afternoon]", "[evening]"];
const MARKER: &str = ".greetings_initialized";

/// 0 = morning (5-11), 1 = afternoon (12-17), 2 = evening.
pub fn period_for_hour(hour: u32) -> usize {
    match hour {
        5..=11 => 0,
        12..=17 => 1,
        _ => 2,
    }
}

pub type Pools = [Vec<String>; 3];

pub fn defaults() -> Pools {
    [MORNING, AFTERNOON, EVENING].map(|p| p.iter().map(|s| s.to_string()).collect())
}

pub fn render(pools: &Pools) -> String {
    let mut out = String::from(
        "# Editable greeting message bank for RemindMe.\n# Blank lines and lines starting with # are ignored.\n\n",
    );
    for (i, header) in SECTIONS.iter().enumerate() {
        out += header;
        out.push('\n');
        for line in &pools[i] {
            out += line;
            out.push('\n');
        }
        if i < 2 {
            out.push('\n');
        }
    }
    out
}

/// Sections that are missing or empty keep their defaults.
pub fn parse(text: &str) -> Pools {
    let mut result = defaults();
    let mut loaded: Pools = Default::default();
    let mut section: Option<usize> = None;
    for line in text.lines().map(str::trim) {
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        if let Some(i) = SECTIONS.iter().position(|s| *s == line) {
            section = Some(i);
        } else if let Some(i) = section {
            loaded[i].push(line.to_string());
        }
    }
    for (i, pool) in loaded.into_iter().enumerate() {
        if !pool.is_empty() {
            result[i] = pool;
        }
    }
    result
}

/// Loads `dir/greetings.txt`, regenerating it with defaults when it was deleted.
pub fn load(dir: &Path) -> Pools {
    let path = dir.join("greetings.txt");
    if !path.exists() {
        let _ = fs::create_dir_all(dir);
        let _ = fs::write(&path, render(&defaults()));
    }
    let _ = fs::write(dir.join(MARKER), "");
    fs::read_to_string(&path)
        .map(|t| parse(&t))
        .unwrap_or_else(|_| defaults())
}
