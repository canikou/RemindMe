# RemindMe handoff (Polaris, updated 2026-11-18, Kyo away)

## Latest: Linux / Hyprland work on branch `fix/flap-overlap` (pushed, NOT merged)

- Commits on top of main: `043df8a` flap tiles flip without overlapping digits; `aaa9b0a` popup + overlay float on tiling WMs (EWMH hints before map, new `src/wm.rs`) and a compact main window under 760 px; `0d2d4ed` Hyprland IPC speaks the Lua dispatch syntax (0.55+ Lua configs), with the classic syntax as a fallback. CI green (run 44237221186). CHANGELOG `## Unreleased` lists them.
- Kyo's Terra (CachyOS, Hyprland 0.57.2, Lua config) runs the `0d2d4ed` AppImage (sha256 1d5ade88...5848). Previous builds: `~/Applications/RemindMe-2.0.3-x86_64.AppImage.bak-20261118` (released 2.0.3) and `.bak-20261118b` (aaa9b0a). Data backup: `~/.local/share/remindme-swap-backup-20261118-122009`.
- Desktop test 2026-11-18 (sandboxed second instance, USER=rmsandbox, temp HOME): popup is a floating, pinned, focused 560x330 dialog; Done click -> `[debug] PopupDone` and it closes; Enter closes it too. No wm.rs errors in the debug log.
- Lead on Kyo's "Done doesn't dismiss": under XWayland a popup that opens under a still pointer gets no pointer-enter, so the first click can miss until the mouse moves. Seen with synthetic input only. Needs Kyo with a real mouse on the new build; if still broken, run with `REMINDME_DEBUG=1` and check whether `PopupDone` is logged.
- How to check: `cargo test --bin remindme flap` / `wm` (CI; too heavy to link on Polaris), and the desktop recipe above (book a DESKTOP slot with Terra Alpha).

## Decisions for Kyo (away mode)

- Tested on Terra with a sandboxed second instance instead of adding a reminder to Kyo's list. Nothing to undo.
- Installed the `0d2d4ed` test build over the aaa9b0a test build Kyo had asked for. Undo: rename `.bak-20261118` (2.0.3) back.

## Queued for Kyo

- Merge `fix/flap-overlap` to main and release (2.0.4) once he confirms the popup and Done on his mouse. Recommendation: merge and release; the release-after-bump routine applies after he approves the bump.

---

# Earlier: Terra → Polaris, 2026-11-15

## Where things stand

- `main` = `e33e67e` "Bump version to 2.0.3". Tag `v2.0.3` is pushed.
- The GitHub release v2.0.3 is published with 3 files from CI: `RemindMe-2.0.3-setup.exe`, `RemindMe-2.0.3-windows-portable.zip` and `RemindMe-2.0.3-x86_64.AppImage`.
- What 2.0.3 contains is listed in `CHANGELOG.md` under `## 2.0.3 - 2026-11-15`. It has the night-audit fixes plus 3 items Kyo approved:
  - snooze keeps the original schedule (`Reminder::snoozed_from`)
  - save errors wait until the open dialog closes
  - a reminders file from a newer version is backed up before it is overwritten
- Nothing is in flight. No local-only branches or uncommitted work.
- 2.0.3 is **not** installed on Kyo's PC (Terra). Kyo hasn't asked for it this time.

## Next steps

- No queued RemindMe work. Wait for Kyo.
- If Kyo asks to install 2.0.3 on Terra, run the setup.exe from the release silently, or update through the app's "Check for Updates". This needs a DESKTOP slot from Terra Manager.
- The 3 approved changes are covered by tests only. Nobody has tried them by hand in the running app. If Kyo wants a hand check, it needs a DESKTOP slot on Terra.

## Open decisions for Kyo

None pending. Already decided:
- The GPU renderer (wgpu) stays the default.
- `ICED_BACKEND=tiny-skia` remains a user override. It starts faster and uses less RAM, but uses about 3x the CPU while the countdown animates.

## Build, test and release

- Rust edition 2024, stable toolchain. The code uses `slice::as_chunks`, which needs Rust 1.88 or newer.
- Checks, the same as CI:
  - `cargo fmt --check`
  - `cargo clippy --locked --all-targets -- -D warnings`
  - `cargo test --locked`
- Release:
  1. Bump `version` in `Cargo.toml`, the `remindme` entry in `Cargo.lock`, `Current version:` in `README.md`, and rename `## Unreleased` in `CHANGELOG.md` to `## X.Y.Z - <date>`.
  2. Commit "Bump version to X.Y.Z", then push main.
  3. Push the lightweight tag `vX.Y.Z` on that commit. CI checks that the tag matches `Cargo.toml`.
  4. Wait for CI on the main push to go green. Never release from a red run.
  5. Run `gh run download <id> -n RemindMe-windows-packages` and `-n RemindMe-linux-packages`.
  6. Run `gh release create vX.Y.Z --title "RemindMe vX.Y.Z"`. Use the version's CHANGELOG bullets as notes, without the `###` headings, and attach the 3 files.
- CI is `.github/workflows/ci.yml`. It runs on every branch push.
  - The Windows job does fmt, clippy, test, release build, the portable zip, Inno Setup (installed via choco), the setup.exe, and smoke tests of both.
  - The Linux job (ubuntu-22.04, the oldest glibc we support) does clippy, test, release build, `scripts/package-appimage.sh`, and a smoke test of the AppImage under xvfb.
- Packaging scripts are in `scripts/`:
  - `package-portable-release.ps1` and `package-setup-release.ps1` run on Windows.
  - `package-appimage.sh` runs on Linux. It downloads a pinned linuxdeploy, checked by hash, into `.tools/`.

## Windows/Terra vs plain Linux

- **Plain Linux (Polaris is fine):**
  - `cargo check`, `cargo clippy`, `cargo test` and `cargo build --release`.
  - These were verified on Ubuntu 24.04 (WSL) with no extra apt packages, only the base C toolchain and linker. No `-dev` libraries are needed.
  - The tray (ksni) and GUI (iced/winit) libraries load at runtime, not build time.
  - Time-zone tests: running `TZ=America/New_York cargo test` exercises the DST code paths.
- **Linux runtime and AppImage:**
  - Building the AppImage needs `curl` and `sha256sum`. It runs without FUSE because of `APPIMAGE_EXTRACT_AND_RUN`.
  - Running the app headless needs `xvfb` and `libxkbcommon-x11-0`, as in CI's smoke test.
- **Needs Windows (Terra, book a slot with Terra Manager first):**
  - Anything that runs the Windows GUI or tray, the installer, or the portable zip.
  - fmt, clippy and test with `cfg(windows)` code. Polaris can't compile the `tray-icon` and MessageBox paths.
  - For Windows-only code, CI's Windows job is an alternative to Terra: push a branch and read its result.
  - HEAVY slot = cargo builds/tests; use `CARGO_BUILD_JOBS=2` and stop if free RAM drops under 1.5 GB.
  - DESKTOP slot = anything that opens the app's window.
  - Avoid 03:00–03:06 on Terra, when the nightly restart prompt appears.

## Gotchas

- Env vars the app reads (names only):
  - `REMINDME_THEME`
  - `REMINDME_REDUCE_MOTION`
  - `REMINDME_DEBUG` (logs messages)
  - `REMINDME_AFTER_UPDATE` (set by the self-updater)
  - `ICED_BACKEND` (iced renderer override)
  - `APPIMAGE` (set by the AppImage runtime)
  - `USER` and `USERNAME` (single-instance tag)
- The live data file is `Documents/RemindMe/reminders.json`, next to `greetings.txt`. There is no data-dir override.
  - On Terra this is Kyo's real data. Back it up before any GUI test and restore it byte-for-byte afterwards.
  - Test files written by PowerShell 5.1 `Set-Content -Encoding utf8` get a BOM. The app now accepts a BOM, but write test files without one anyway.
- Only one copy runs at a time. It uses a lock file plus a localhost TCP port file (`remindme-<user>.port` in the runtime/temp dir), and later launches send `show`.
- Background work uses detached threads (`app.rs` `blocking()`), not `spawn_blocking`, so Quit never waits on a download.
- GitHub has old remote branches (`rust-rewrite`, `ux/polish-pass` and others). They are history, not work in progress.
