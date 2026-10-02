#!/usr/bin/env bash
# Download the prebuilt libmpv AAR published by jarnedemeulemeester/libmpv-android,
# extract the shared libraries for the ABIs we build and fetch the matching
# mpv headers. Result layout:
#   android-project/app/libs/<abi>/{libmpv,libavcodec,...}.so   (packaged into the APK)
#   mpv-deps/include/mpv/*.h                                    (headers for the build)
set -euo pipefail

MPV_AAR_URL="https://github.com/jarnedemeulemeester/libmpv-android/releases/download/v1.0.0/libmpv-release.aar"
MPV_HEADERS_URL="https://github.com/mpv-player/mpv/archive/refs/tags/v0.41.0.tar.gz"
ABIS="armeabi-v7a arm64-v8a x86_64"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
JNILIBS="$ROOT/android-project/app/libs"
DEPS="$ROOT/mpv-deps"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$JNILIBS" "$DEPS/include"

echo "Downloading libmpv AAR..."
curl -sSL "$MPV_AAR_URL" -o "$TMP/libmpv.aar"
unzip -q "$TMP/libmpv.aar" -d "$TMP/aar"

for abi in $ABIS; do
    if [ ! -d "$TMP/aar/jni/$abi" ]; then
        echo "WARNING: ABI $abi missing in AAR, skipping" >&2
        continue
    fi
    mkdir -p "$JNILIBS/$abi"
    for so in "$TMP/aar/jni/$abi/"*.so; do
        name="$(basename "$so")"
        case "$name" in
            # libc++_shared.so comes from our own NDK build,
            # libplayer.so is the AAR's Java wrapper, not used by wiliwili
            libc++_shared.so|libplayer.so) continue ;;
        esac
        cp "$so" "$JNILIBS/$abi/"
    done
    echo "Prepared $abi: $(ls "$JNILIBS/$abi" | tr '\n' ' ')"
done

echo "Downloading mpv headers..."
curl -sSL "$MPV_HEADERS_URL" -o "$TMP/mpv.tar.gz"
mkdir -p "$TMP/mpv-src"
# cd into the target instead of using `tar -C <abs path>`: on Git Bash for Windows
# GNU tar reads the "C:" drive prefix as an rsh host and fails with
# "Cannot connect to C: resolve failed".
(
    cd "$TMP/mpv-src"
    tar -xzf "$TMP/mpv.tar.gz" --strip-components=1
)
cp -r "$TMP/mpv-src/include/mpv" "$DEPS/include/"

echo "Done."
