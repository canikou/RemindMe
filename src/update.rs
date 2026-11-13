// SPDX-License-Identifier: MIT
//! GitHub Releases update rules. Windows keeps the 1.x contract (prefer `*-setup.exe`);
//! Linux takes the `.AppImage`. Linux asset names never end in .exe/.msi/.zip, so 1.x clients ignore them.

use serde_json::Value;

pub const LATEST_RELEASE_API_URL: &str =
    "https://api.github.com/repos/canikou/RemindMe/releases/latest";
pub const RELEASES_PAGE_URL: &str = "https://github.com/canikou/RemindMe/releases";

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Platform {
    Windows,
    Linux,
}

impl Platform {
    pub const CURRENT: Platform = if cfg!(windows) {
        Platform::Windows
    } else {
        Platform::Linux
    };
}

pub fn parse_version_triplet(text: &str) -> Option<(i64, i64, i64)> {
    let t = text.trim();
    let t = t.strip_prefix(['v', 'V']).unwrap_or(t);
    let parts: Vec<&str> = t.split('.').collect();
    if parts.len() != 3 {
        return None;
    }
    Some((
        parts[0].parse().ok()?,
        parts[1].parse().ok()?,
        parts[2].parse().ok()?,
    ))
}

pub fn is_remote_version_newer(remote: &str, current: &str) -> bool {
    match (
        parse_version_triplet(remote),
        parse_version_triplet(current),
    ) {
        (Some(r), Some(c)) => r > c,
        _ => false,
    }
}

/// Ok(true) when `remote` is newer; Err when either tag isn't `major.minor.patch`,
/// so an odd tag (`v2.1`, `2.1.0-rc1`) is reported instead of passing for "up to date".
pub fn compare_remote_tag(remote: &str, current: &str) -> Result<bool, String> {
    let parse = |t: &str| {
        parse_version_triplet(t)
            .ok_or_else(|| format!("Unrecognized version tag \"{}\".", t.trim()))
    };
    Ok(parse(remote)? > parse(current)?)
}

pub fn is_installer_asset_name(name: &str, platform: Platform) -> bool {
    let n = name.to_lowercase();
    match platform {
        Platform::Windows => {
            n.ends_with(".msi")
                || (n.ends_with(".exe") && (n.contains("setup") || n.contains("installer")))
        }
        Platform::Linux => n.ends_with(".appimage"),
    }
}

pub fn update_asset_priority(name: &str, platform: Platform) -> i32 {
    let n = name.to_lowercase();
    match platform {
        Platform::Windows => {
            if is_installer_asset_name(name, platform) && n.ends_with(".exe") {
                300
            } else if n.ends_with(".msi") {
                250
            } else if n.ends_with(".exe") {
                200
            } else if n.ends_with(".zip") {
                100
            } else {
                0
            }
        }
        Platform::Linux => {
            if n.ends_with(".appimage") {
                300
            } else {
                0
            }
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct UpdateAsset {
    pub name: String,
    pub download_url: String,
    pub sha256_hex: String,
    pub is_installer: bool,
}

/// Highest-priority asset; later assets win ties, as in 1.x.
pub fn pick_best_release_asset(assets: &[Value], platform: Platform) -> Option<UpdateAsset> {
    let mut best: Option<(i32, UpdateAsset)> = None;
    for o in assets.iter().filter_map(Value::as_object) {
        let name = o
            .get("name")
            .and_then(Value::as_str)
            .unwrap_or_default()
            .trim();
        let url = o
            .get("browser_download_url")
            .and_then(Value::as_str)
            .unwrap_or_default()
            .trim();
        if name.is_empty() || !url.starts_with("https://") {
            continue;
        }
        let priority = update_asset_priority(name, platform);
        if priority <= 0 || best.as_ref().is_some_and(|(p, _)| priority < *p) {
            continue;
        }
        let digest = o
            .get("digest")
            .and_then(Value::as_str)
            .unwrap_or_default()
            .trim();
        let sha256_hex = digest
            .get(..7)
            .filter(|p| p.eq_ignore_ascii_case("sha256:"))
            .map(|_| digest[7..].trim().to_lowercase())
            .unwrap_or_default();
        // No verifiable checksum, no update: never offer an unverifiable download.
        if sha256_hex.len() != 64 || !sha256_hex.bytes().all(|b| b.is_ascii_hexdigit()) {
            continue;
        }
        best = Some((
            priority,
            UpdateAsset {
                name: name.into(),
                download_url: url.into(),
                sha256_hex,
                is_installer: is_installer_asset_name(name, platform),
            },
        ));
    }
    best.map(|(_, a)| a)
}
