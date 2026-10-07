#!/bin/sh
# Build lighthouse-pm natively inside an Arch Linux aarch64 container (the
# Frame's distro) so the binary links against the exact libraries the device
# ships and runs there without bundling anything.
#
# Invoked by CI as:
#   docker run --rm --platform linux/arm64 -v "$GITHUB_WORKSPACE:/src" -w /src \
#     archlinux/archlinux:latest /bin/bash /src/build_env/build_arch_aarch64.sh
set -eu

echo "=== running on: $(arch) / $(uname -m) ==="

echo "==> installing build deps + Qt6/KF6/Kirigami/BlueZ"
pacman -Sy --noconfirm
pacman -S --noconfirm --needed base-devel cmake ninja \
  qt6-qtbase kf6-core kf6-kconfig kf6-ki18n kirigami bluez

echo "==> installed versions"
pacman -Q qt6-qtbase kf6-core kf6-kconfig kf6-ki18n kirigami bluez 2>/dev/null | sed 's/^/  /' || true

echo "==> cmake configure"
cmake -S /src -B /src/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/lighthouse-pm

echo "==> build"
cmake --build /src/build -j"$(nproc)"

echo "==> NEEDED shared libraries of the built binary"
readelf -d /src/build/lighthouse-pm | grep -E "NEEDED" || true

echo "==> smoke test (offscreen, 10s)"
rc=0
timeout 10 env QT_QPA_PLATFORM=offscreen /src/build/lighthouse-pm >/src/smoke.log 2>&1 || rc=$?
case "$rc" in
  124|143)
    echo "OK: app ran until the timeout (event loop alive)"
    ;;
  *)
    echo "--- smoke log ---"
    cat /src/smoke.log
    echo "SMOKE FAIL (rc=$rc)"
    exit 1
    ;;
esac
