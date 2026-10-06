#!/bin/sh
# Cross-build lighthouse-pm for aarch64 Alpine/KDE (musl).
#
# Host: x86_64 Alpine with the aarch64-none-elf cross toolchain installed:
#   apk add gcc-aarch64-none-elf g++-aarch64-none-elf binutils-aarch64-none-elf \
#           newlib-aarch64-none-elf cmake make ninja qemu-aarch64 python3 file
# Sysroot: /opt/aarch64-sysroot (aarch64 Alpine packages extracted; see
# build_env/build_sysroot.py), then wrap_sysroot_qemu.sh for the Qt tools.
set -e

SYSROOT=${SYSROOT:-/opt/aarch64-sysroot}
[ -d "$SYSROOT/usr/lib" ] || { echo "sysroot missing: $SYSROOT (run build_env/build_sysroot.py)"; exit 1; }

BUILD_DIR="$(dirname "$0")/build-aarch64"
JOBS=$(nproc 2>/dev/null || echo 4)

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCROSS_SYSROOT="$SYSROOT" \
    -DCMAKE_TOOLCHAIN_FILE="$(dirname "$0")/cmake/aarch64-linux-musl.cmake"

make -j"$JOBS"

echo
echo "Built: $BUILD_DIR/lighthouse-pm (aarch64)"
file ./lighthouse-pm 2>/dev/null || true
echo
echo "Smoke test under qemu (offscreen, 10 s):"
rc=0
out=$(timeout 10 qemu-aarch64 -L "$SYSROOT" \
    -E QT_QPA_PLATFORM=offscreen \
    -E LD_LIBRARY_PATH="$SYSROOT/usr/lib" \
    -E QT_PLUGIN_PATH="$SYSROOT/usr/lib/qt6/plugins" \
    -E QML_IMPORT_PATH="$SYSROOT/usr/lib/qt6/qml" \
    ./lighthouse-pm 2>&1) || rc=$?
# 124 (GNU timeout) or 143 (SIGTERM): the app was killed by the timeout
case "$rc" in
    124|143)
        echo "OK: app ran until the timeout (event loop alive)"
        ;;
    *)
        echo "$out"
        echo "FAIL: app exited early (rc=$rc)"
        exit 1
        ;;
esac
