#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Builds dist/RemindMe-<version>-x86_64.AppImage from target/release/RemindMe.
# Build on the oldest distro you support: the AppImage needs that glibc or newer.
set -euo pipefail
cd "$(dirname "$0")/.."

version=$(sed -n 's/^version = "\(.*\)"/\1/p' Cargo.toml | head -1)
binary=${1:-target/release/RemindMe}
[[ -x "$binary" ]] || { echo "Release binary not found: $binary (run: cargo build --release)" >&2; exit 1; }

tools=.tools
mkdir -p "$tools" dist
# Pinned release, verified by hash: bump both together.
linuxdeploy_tag=1-alpha-20251107-1
linuxdeploy_sha256=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
linuxdeploy="$tools/linuxdeploy-$linuxdeploy_tag-x86_64.AppImage"
if [[ ! -x "$linuxdeploy" ]]; then
    curl -fsSL -o "$linuxdeploy.part" "https://github.com/linuxdeploy/linuxdeploy/releases/download/$linuxdeploy_tag/linuxdeploy-x86_64.AppImage"
    echo "$linuxdeploy_sha256  $linuxdeploy.part" | sha256sum -c - || { rm -f "$linuxdeploy.part"; exit 1; }
    chmod +x "$linuxdeploy.part"
    mv "$linuxdeploy.part" "$linuxdeploy"
fi

appdir=$(mktemp -d)/AppDir
trap 'rm -rf "$(dirname "$appdir")"' EXIT
# Runs without FUSE (CI containers, minimal systems).
export APPIMAGE_EXTRACT_AND_RUN=1 LINUXDEPLOY_OUTPUT_VERSION="$version"
export OUTPUT="dist/RemindMe-$version-x86_64.AppImage"
rm -f "$OUTPUT"
icon_dir=$(mktemp -d)
cp resources/icons/app.png "$icon_dir/remindme.png"
"$linuxdeploy" --appdir "$appdir" \
    --executable "$binary" \
    --desktop-file resources/remindme.desktop \
    --icon-file "$icon_dir/remindme.png" \
    --output appimage
rm -rf "$icon_dir"
echo "AppImage: $OUTPUT"
