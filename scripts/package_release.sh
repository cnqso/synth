#!/usr/bin/env bash

set -euo pipefail

log() {
    printf '%s\n' "$*" >&2
}

if [[ $# -ne 2 ]]; then
    printf 'usage: %s <platform-label> <version>\n' "${0##*/}" >&2
    exit 1
fi

platform_label="$1"
version="$2"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
make_cmd="${MAKE:-make}"

if ! command -v "$make_cmd" >/dev/null 2>&1 && command -v mingw32-make >/dev/null 2>&1; then
    make_cmd="mingw32-make"
fi

case "$platform_label" in
    linux-*) platform_family="linux" ;;
    macos-*) platform_family="macos" ;;
    windows-*) platform_family="windows" ;;
    *)
        printf 'unsupported platform label: %s\n' "$platform_label" >&2
        exit 1
        ;;
esac

app_name="cnqsosynth"
binary_name="$app_name"
if [[ "$platform_family" == "windows" ]]; then
    binary_name="$app_name.exe"
fi

stage_root="$repo_root/dist/$platform_label"
bundle_name="$app_name-$version-$platform_label"
bundle_dir="$stage_root/$bundle_name"

rm -rf "$bundle_dir"
mkdir -p "$stage_root"

log "Building $binary_name for $platform_label"
"$make_cmd" -C "$repo_root" clean >/dev/null
"$make_cmd" -C "$repo_root" VERSION="$version" >/dev/null

copy_common_files() {
    cp "$repo_root/README.md" "$1/"
}

package_linux() {
    local sdl_prefix

    mkdir -p "$bundle_dir/lib"
    cp "$repo_root/$binary_name" "$bundle_dir/"
    cp "$repo_root/Nintendo-DS-BIOS.ttf" "$bundle_dir/"
    copy_common_files "$bundle_dir"

    sdl_prefix="$(pkg-config --variable=prefix sdl3)"
    if [[ -d "$sdl_prefix/lib" ]]; then
        find "$sdl_prefix/lib" -maxdepth 1 -name 'libSDL3.so*' -exec cp -a {} "$bundle_dir/lib/" \;
    fi

    if command -v patchelf >/dev/null 2>&1; then
        patchelf --set-rpath '$ORIGIN/lib' "$bundle_dir/$binary_name"
    fi

    tar -C "$stage_root" -czf "$stage_root/$bundle_name.tar.gz" "$bundle_name"
    printf '%s\n' "$stage_root/$bundle_name.tar.gz"
}

package_macos() {
    local app_dir contents_dir bin_dir resources_dir frameworks_dir app_bin linked_sdl bundled_sdl plist_path sdl_libdir resolved_sdl

    app_dir="$stage_root/$bundle_name.app"
    contents_dir="$app_dir/Contents"
    bin_dir="$contents_dir/MacOS"
    resources_dir="$contents_dir/Resources"
    frameworks_dir="$contents_dir/Frameworks"
    app_bin="$bin_dir/$app_name"
    plist_path="$contents_dir/Info.plist"

    rm -rf "$app_dir"
    mkdir -p "$bin_dir" "$resources_dir" "$frameworks_dir"

    cp "$repo_root/$binary_name" "$app_bin"
    cp "$repo_root/Nintendo-DS-BIOS.ttf" "$resources_dir/"
    cp "$repo_root/README.md" "$resources_dir/"

    linked_sdl="$(otool -L "$repo_root/$binary_name" | awk '/libSDL3.*dylib/ { print $1; exit }')"
    sdl_libdir="$(pkg-config --variable=libdir sdl3 2>/dev/null || true)"
    resolved_sdl=""

    if [[ -n "${sdl_libdir:-}" && -f "$sdl_libdir/libSDL3.0.dylib" ]]; then
        resolved_sdl="$sdl_libdir/libSDL3.0.dylib"
    elif [[ -n "${linked_sdl:-}" && "$linked_sdl" = /* && -f "$linked_sdl" ]]; then
        resolved_sdl="$linked_sdl"
    elif [[ -n "${linked_sdl:-}" && -n "${sdl_libdir:-}" && -f "$sdl_libdir/$(basename "$linked_sdl")" ]]; then
        resolved_sdl="$sdl_libdir/$(basename "$linked_sdl")"
    fi

    if [[ -n "${linked_sdl:-}" && -n "${resolved_sdl:-}" ]]; then
        bundled_sdl="$frameworks_dir/$(basename "$linked_sdl")"
        cp -L "$resolved_sdl" "$bundled_sdl"
        install_name_tool -change "$linked_sdl" "@executable_path/../Frameworks/$(basename "$bundled_sdl")" "$app_bin"
    elif [[ -n "${linked_sdl:-}" ]]; then
        printf 'Unable to resolve SDL runtime for macOS bundle (link name: %s)\n' "$linked_sdl" >&2
        exit 1
    fi

    cat >"$plist_path" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>$app_name</string>
    <key>CFBundleIdentifier</key>
    <string>com.cnq.$app_name</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>$app_name</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>$version</string>
    <key>CFBundleVersion</key>
    <string>$version</string>
    <key>LSMinimumSystemVersion</key>
    <string>13.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

    if command -v codesign >/dev/null 2>&1; then
        codesign --force --deep --sign - "$app_dir" >/dev/null
    fi

    rm -f "$stage_root/$bundle_name.zip"
    (cd "$stage_root" && COPYFILE_DISABLE=1 zip -qr "$bundle_name.zip" "$(basename "$app_dir")")
    printf '%s\n' "$stage_root/$bundle_name.zip"
}

package_windows() {
    local sdl_prefix windows_dll

    mkdir -p "$bundle_dir"
    cp "$repo_root/$binary_name" "$bundle_dir/"
    cp "$repo_root/Nintendo-DS-BIOS.ttf" "$bundle_dir/"
    copy_common_files "$bundle_dir"

    sdl_prefix="$(pkg-config --variable=prefix sdl3)"
    windows_dll="$(find "$sdl_prefix" -path '*/bin/SDL3.dll' | head -n 1)"
    if [[ -z "${windows_dll:-}" ]]; then
        printf 'SDL3.dll not found under %s\n' "$sdl_prefix" >&2
        exit 1
    fi

    cp "$windows_dll" "$bundle_dir/"
    (cd "$stage_root" && zip -qr "$bundle_name.zip" "$bundle_name")
    printf '%s\n' "$stage_root/$bundle_name.zip"
}

case "$platform_family" in
    linux) package_linux ;;
    macos) package_macos ;;
    windows) package_windows ;;
esac
