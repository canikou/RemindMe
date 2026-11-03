// SPDX-License-Identifier: MIT
//! Network side of updates: GitHub metadata, verified download, handing off to the new version.
//! Blocking calls; the app runs them off the UI thread.

use remindme::update::{self, Platform, UpdateAsset};
use sha2::{Digest, Sha256};
use std::io::Read;
use std::path::{Path, PathBuf};
use std::time::Duration;

const CHECK_INTERVAL_SECONDS: i64 = 24 * 60 * 60;

#[derive(Clone, Debug)]
pub enum CheckResult {
    UpToDate,
    Available {
        tag: String,
        release_url: String,
        asset: Option<UpdateAsset>,
    },
}

fn agent(timeout: u64) -> ureq::Agent {
    ureq::Agent::config_builder()
        .timeout_global(Some(Duration::from_secs(timeout)))
        .user_agent(format!("{}/{}", remindme::APP_NAME, remindme::APP_VERSION))
        .build()
        .into()
}

pub fn check() -> Result<CheckResult, String> {
    record_check_now();
    let body: serde_json::Value = agent(15)
        .get(update::LATEST_RELEASE_API_URL)
        .header("Accept", "application/vnd.github+json")
        .call()
        .map_err(|e| format!("Failed to check updates:\n{e}"))?
        .body_mut()
        .read_json()
        .map_err(|_| "Update metadata was invalid.".to_string())?;
    let tag = body
        .get("tag_name")
        .and_then(|v| v.as_str())
        .unwrap_or_default()
        .trim()
        .to_string();
    if tag.is_empty() {
        return Err("Latest release did not include a version tag.".into());
    }
    if !update::is_remote_version_newer(&tag, remindme::APP_VERSION) {
        return Ok(CheckResult::UpToDate);
    }
    let release_url = body
        .get("html_url")
        .and_then(|v| v.as_str())
        .unwrap_or(update::RELEASES_PAGE_URL)
        .to_string();
    let assets = body
        .get("assets")
        .and_then(|v| v.as_array())
        .cloned()
        .unwrap_or_default();
    Ok(CheckResult::Available {
        tag,
        release_url,
        asset: update::pick_best_release_asset(&assets, Platform::CURRENT),
    })
}

/// Downloads next to where it will be installed (so the final rename stays on one filesystem), checking the digest.
pub fn download(asset: &UpdateAsset) -> Result<PathBuf, String> {
    let dir = match appimage_path() {
        Some(p) => p
            .parent()
            .map(Path::to_path_buf)
            .unwrap_or_else(std::env::temp_dir),
        None => std::env::temp_dir(),
    };
    let safe_name: String = Path::new(&asset.name)
        .file_name()
        .map(|n| n.to_string_lossy().into_owned())
        .unwrap_or_else(|| "update.bin".into());
    let path = dir.join(format!(".{}-{}.part", remindme::APP_NAME, safe_name));
    let final_path = if appimage_path().is_some() {
        path.clone()
    } else {
        dir.join(format!("{}-{}", remindme::APP_NAME, safe_name))
    };

    let mut reader = agent(600)
        .get(&asset.download_url)
        .call()
        .map_err(|e| format!("Failed to download update:\n{e}"))?
        .into_body()
        .into_reader();
    let mut bytes = Vec::new();
    reader
        .read_to_end(&mut bytes)
        .map_err(|e| format!("Failed to download update:\n{e}"))?;

    if !asset.sha256_hex.is_empty() {
        let actual: String = Sha256::digest(&bytes)
            .iter()
            .map(|b| format!("{b:02x}"))
            .collect();
        if actual != asset.sha256_hex.to_lowercase() {
            return Err("Downloaded update failed checksum verification.".into());
        }
    }
    std::fs::write(&final_path, &bytes)
        .map_err(|_| "Failed to open local file for update download.".to_string())?;
    Ok(final_path)
}

/// The AppImage this process runs from, if any.
pub fn appimage_path() -> Option<PathBuf> {
    if !cfg!(target_os = "linux") {
        return None;
    }
    std::env::var_os("APPIMAGE")
        .map(PathBuf::from)
        .filter(|p| p.is_file())
}

/// Starts the new version. The caller quits on success.
pub fn install(downloaded: &Path) -> Result<(), String> {
    #[cfg(windows)]
    {
        let lower = downloaded.to_string_lossy().to_lowercase();
        let mut cmd = if lower.ends_with(".msi") {
            let mut c = std::process::Command::new("msiexec");
            c.arg("/i").arg(downloaded);
            c
        } else {
            std::process::Command::new(downloaded)
        };
        cmd.spawn()
            .map(|_| ())
            .map_err(|_| "Failed to launch installer.".into())
    }
    #[cfg(not(windows))]
    {
        use std::os::unix::fs::PermissionsExt;
        let target = appimage_path()
            .ok_or("Not running from an AppImage; install the download manually.")?;
        std::fs::set_permissions(downloaded, std::fs::Permissions::from_mode(0o755))
            .map_err(|e| e.to_string())?;
        // Replacing a running AppImage is safe: the old process keeps its open inode.
        std::fs::rename(downloaded, &target)
            .map_err(|e| format!("Couldn't replace {}:\n{e}", target.display()))?;
        std::process::Command::new(&target)
            .spawn()
            .map(|_| ())
            .map_err(|_| "Failed to start the new version.".into())
    }
}

// ---- once-a-day startup check ----

fn settings_path() -> Option<PathBuf> {
    dirs::config_dir().map(|d| d.join(remindme::APP_NAME).join("settings.json"))
}

pub fn should_check_now() -> bool {
    let last = settings_path()
        .and_then(|p| std::fs::read_to_string(p).ok())
        .and_then(|t| serde_json::from_str::<serde_json::Value>(&t).ok())
        .and_then(|v| v.get("last_update_check_utc")?.as_i64());
    last.is_none_or(|t| chrono::Utc::now().timestamp() - t >= CHECK_INTERVAL_SECONDS)
}

fn record_check_now() {
    if let Some(p) = settings_path() {
        let _ = std::fs::create_dir_all(p.parent().unwrap_or(Path::new(".")));
        let _ = std::fs::write(
            p,
            serde_json::json!({ "last_update_check_utc": chrono::Utc::now().timestamp() })
                .to_string(),
        );
    }
}
