// SPDX-License-Identifier: MIT
// Embeds the app icon into RemindMe.exe (shown in Explorer, the taskbar and the installer).
fn main() {
    if std::env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("windows") {
        embed_resource::compile("resources/app.rc", embed_resource::NONE)
            .manifest_optional()
            .unwrap();
    }
}
