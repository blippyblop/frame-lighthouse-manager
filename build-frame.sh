#!/bin/sh
# Build lighthouse-pm against the Valve Frame firmware sysroot
# (build_env/frame-sysroot): the device's own gcc 15.1.1 / binutils / glibc /
# Qt 6.8 / KF6 / Kirigami, executed through the qemu-aarch64 copy bundled
# inside the sysroot. Output is an aarch64 binary that matches the firmware
# ABI exactly.
#
# Host needs (x86_64):
#   - a native cmake  (default: /workspace/tools/bin/cmake-native)
#   - a ninja          (default: /workspace/tools/bin/ninja; may be the
#                       qemu-emulated one from the firmware sysroot)
#   - gettext msgfmt/msgmerge reachable in PATH (the tools dir ships
#                       qemu wrappers for the firmware's own binaries)
# No host qemu-aarch64 is required.
#
#   usage: sh build-frame.sh
set -e

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SYSROOT="$ROOT/build_env/frame-sysroot"
TOOLS="${TOOLS:-/workspace/tools/bin}"
BUILD="$ROOT/build-frame"

[ -d "$SYSROOT/usr/lib/qt6" ] || { echo "frame-sysroot missing under build_env/ (see build_env/setup_frame_includes.sh)"; exit 1; }

CMAKE="${CMAKE:-$TOOLS/cmake-native}"
command -v "$CMAKE" >/dev/null || CMAKE=cmake
NINJA="${NINJA:-$TOOLS/ninja}"
command -v "$NINJA" >/dev/null || NINJA=ninja

# PATH order matters: the qemu'd firmware gcc driver PATH-searches for
# as/ld, so the sysroot bin dirs must come before any host toolchain.
# The tools dir (cmake/native/msg*) must come first so `cmake` itself
# resolves to a host-runnable binary.
export PATH="$TOOLS:$SYSROOT/usr/bin:$SYSROOT/bin:$PATH"

rm -rf "$BUILD"
"$CMAKE" -B "$BUILD" -S "$ROOT" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/build_env/toolchain_frame.cmake" \
    -DCMAKE_MAKE_PROGRAM="$NINJA" \
    -DCMAKE_CROSSCOMPILING_EMULATOR="$SYSROOT/usr/bin/qemu-aarch64;-L;$SYSROOT"

"$CMAKE" --build "$BUILD"

echo
echo "Built: $BUILD/lighthouse-pm (aarch64, firmware ABI)"
echo "Smoke test under qemu (offscreen, 12 s):"
rc=0
out=$(timeout 12 "$SYSROOT/usr/bin/qemu-aarch64" -L "$SYSROOT" \
    -E QT_QPA_PLATFORM=offscreen \
    -E LD_LIBRARY_PATH="$SYSROOT/usr/lib" \
    -E QT_PLUGIN_PATH="$SYSROOT/usr/lib/qt6/plugins" \
    -E QML_IMPORT_PATH="$SYSROOT/usr/lib/qt6/qml" \
    -E HOME=/tmp \
    "$BUILD/lighthouse-pm" 2>&1) || rc=$?
case "$rc" in
    124|143) echo "OK: app ran until the timeout (event loop alive)" ;;
    *)
        echo "$out"
        echo "FAIL: app exited early (rc=$rc)"
        exit 1
        ;;
esac
