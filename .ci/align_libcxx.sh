#!/usr/bin/env bash
# Replace the NDK's libc++_shared.so with the one shipped alongside the prebuilt
# libmpv, for the ABIs we package.
#
# Why this is required and not just an optimisation:
#
#   1. Symbol completeness. libmpv.so imports
#        std::__ndk1::__from_chars_floating_point<float|double>(...)
#      which is only exported by the newer libc++ bundled in the AAR. The
#      libc++_shared.so that ships with NDK r26 does not export it, so libmpv.so
#      would fail to resolve those symbols at load time.
#   2. 16 KB page alignment. The AAR copy has p_align 0x4000 on its LOAD
#      segments for arm64-v8a / x86_64; the NDK r26 copy has 0x1000, which
#      Android 15+ 16 KB page kernels reject.
#
# AGP packages the STL from the NDK sysroot, so the swap has to happen there.
# Everything is printed so the CI log shows exactly what was replaced, and the
# script fails loudly if it could not install the library for an ABI.
set -euo pipefail

ABIS="${ABIS:-arm64-v8a armeabi-v7a x86_64}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC_ROOT="$ROOT/android-project/app/libs"

SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
if [ -z "$SDK" ]; then
    echo "ERROR: neither ANDROID_SDK_ROOT nor ANDROID_HOME is set" >&2
    exit 1
fi

NDK_VERSION="$(sed -n 's/.*ndkVersion[[:space:]]*"\([^"]*\)".*/\1/p' \
    "$ROOT/android-project/app/build.gradle" | head -1)"
if [ -z "$NDK_VERSION" ]; then
    echo "ERROR: could not read ndkVersion from android-project/app/build.gradle" >&2
    exit 1
fi

NDK="$SDK/ndk/$NDK_VERSION"
if [ ! -d "$NDK" ]; then
    echo "ERROR: NDK not found at $NDK" >&2
    exit 1
fi
echo "NDK        = $NDK"
echo "NDK version= $NDK_VERSION"
echo

# ABI -> NDK target triple, used to recognise the sysroot directory
triple_of() {
    case "$1" in
        arm64-v8a)   echo "aarch64-linux-android" ;;
        armeabi-v7a) echo "arm-linux-androideabi" ;;
        x86_64)      echo "x86_64-linux-android" ;;
        x86)         echo "i686-linux-android" ;;
        *)           echo "" ;;
    esac
}

status=0
for abi in $ABIS; do
    src="$SRC_ROOT/$abi/libc++_shared.so"
    if [ ! -f "$src" ]; then
        echo "ERROR: $src missing - run .ci/prepare_mpv.sh first" >&2
        status=1
        continue
    fi

    triple="$(triple_of "$abi")"
    echo "--- $abi (triple $triple) ---"
    echo "    source : $src  sha256=$(sha256sum "$src" | cut -d' ' -f1)"

    replaced=0
    # Every libc++_shared.so under the NDK whose path mentions this ABI's
    # triple, plus the legacy sources/cxx-stl layout keyed by ABI name.
    while IFS= read -r target; do
        [ -n "$target" ] || continue
        case "$target" in
            *"/$triple/"*|*"/$abi/"*)
                echo "    old    : $target  sha256=$(sha256sum "$target" | cut -d' ' -f1)"
                cp -f "$src" "$target"
                echo "    new    : $target  sha256=$(sha256sum "$target" | cut -d' ' -f1)"
                replaced=$((replaced + 1))
                ;;
        esac
    done < <(find "$NDK" -name 'libc++_shared.so' -type f 2>/dev/null)

    if [ "$replaced" -eq 0 ]; then
        echo "ERROR: no libc++_shared.so under the NDK matched $abi" >&2
        status=1
    else
        echo "    replaced $replaced file(s)"
    fi
    echo
done

if [ "$status" -ne 0 ]; then
    echo "FAILED: could not install the AAR libc++_shared.so everywhere it is needed" >&2
    exit 1
fi

echo "All ABI libc++_shared.so files now come from the libmpv AAR."
