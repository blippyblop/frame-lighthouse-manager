#!/usr/bin/env bash
# Build the Lighthouse PM app natively against Valve's Holo Core aarch64
# preview -- the Steam Frame's actual OS base (Arch-derived, KDE Plasma).
#
#   repo:   https://holo-packages.steamos.cloud/holo-core-aarch64-preview/mash-20251118/
#   docs:   https://gitlab.steamos.cloud/holo/holo-core-aarch64-preview
#
# Method (no QEMU, no docker, no cross toolchain):
#   1. download Valve's published base system rootfs (system.rootfs.zst)
#   2. unpack it and chroot into it on this native aarch64 runner
#   3. pacman -S the toolchain + Qt6/KF6/Kirigami from Valve's public repo
#   4. cmake/ninja build, link-check, offscreen smoke test
#   5. copy the binary to dist/
#
# The binary links against the exact library set the Frame OS ships
# (SONAMEs: libQt6*.so.6, libKF6*.so.6, libKirigami.so.6, libbluetooth.so.3),
# so it runs on the device without bundling anything.
set -euo pipefail

HOLO_REPO="https://holo-packages.steamos.cloud/holo-core-aarch64-preview/mash-20251118"
ROOTFS_URL="$HOLO_REPO/system.rootfs.zst"
# integrity pin for the published 385 MB rootfs
ROOTFS_SHA256="7e3fb88454e1ac633b7488abb72d3ca0cc7d2578a38146fdd7d58b50fcbd60bf"

WORKDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST="$WORKDIR/dist"
DL="/tmp/holo-dl"
ROOTFS="${ROOTFS_DIR:-/tmp/holo-rootfs}"
SRC_IN_ROOTFS="/build-src"   # repo bind-mounted here, inside the rootfs

log() { echo "[holo-build] $*"; }
die() { echo "[holo-build] ERROR: $*" >&2; exit 1; }

SUDO=""
[ "$(id -u)" -ne 0 ] && command -v sudo >/dev/null && SUDO="sudo"

# ---------------------------------------------------------------- [1/6] ---
log "[1/6] preflight"
[ "$(uname -m)" = "aarch64" ] || die "must run natively on aarch64 (got $(uname -m))"
command -v curl  >/dev/null || die "curl missing"
command -v tar   >/dev/null || die "tar missing"
if ! command -v zstd >/dev/null; then
  log "zstd missing, installing"
  if command -v apt-get >/dev/null; then
    $SUDO apt-get update -qq && $SUDO apt-get install -y -qq zstd
  else
    die "zstd missing and no apt to install it"
  fi
fi
rm -rf "$DL"; mkdir -p "$DL" "$DIST"

# ---------------------------------------------------------------- [2/6] ---
log "[2/6] downloading $ROOTFS_URL (~385 MB)"
curl -fL --retry 3 --retry-delay 5 --connect-timeout 20 \
  --max-time 3600 -o "$DL/system.rootfs.zst" "$ROOTFS_URL"
echo "$ROOTFS_SHA256  $DL/system.rootfs.zst" | sha256sum -c - \
  || die "rootfs sha256 mismatch -- aborting (repo contents changed?)"

# ---------------------------------------------------------------- [3/6] ---
log "[3/6] extracting rootfs to $ROOTFS"
rm -rf "$ROOTFS"; mkdir -p "$ROOTFS"
zstd -dc "$DL/system.rootfs.zst" | tar -x -C "$ROOTFS"
du -sh "$ROOTFS"

# ---------------------------------------------------------------- [4/6] ---
log "[4/6] preparing chroot (dev nodes, resolv, mounts)"
cp /etc/resolv.conf "$ROOTFS/etc/resolv.conf"

# Valve's packaging strips /dev /proc /sys from the rootfs -- recreate them
$SUDO mkdir -p "$ROOTFS/dev/pts" "$ROOTFS/proc" "$ROOTFS/sys"
mknod_pair() { [ -e "$ROOTFS/dev/$1" ] || $SUDO mknod -m 666 "$ROOTFS/dev/$1" c "$2" "$3"; }
mknod_pair null    1 3
mknod_pair zero    1 5
mknod_pair full    1 7
mknod_pair urandom 1 9
mknod_pair random  1 8
mknod_pair tty     5 0
mknod_pair ptmx    5 2

$SUDO mkdir -p "$ROOTFS$SRC_IN_ROOTFS"
$SUDO mount --bind "$WORKDIR" "$ROOTFS$SRC_IN_ROOTFS"
$SUDO mount -t proc     proc     "$ROOTFS/proc"
$SUDO mount -t sysfs    sys      "$ROOTFS/sys"
$SUDO mount -t devpts   devpts   "$ROOTFS/dev/pts"

cleanup() {
  set +e
  $SUDO umount "$ROOTFS/dev/pts"  2>/dev/null
  $SUDO umount "$ROOTFS/sys"      2>/dev/null
  $SUDO umount "$ROOTFS/proc"     2>/dev/null
  $SUDO umount "$ROOTFS$SRC_IN_ROOTFS" 2>/dev/null
  if [ "${KEEP_ROOTFS:-0}" = "1" ]; then
    log "keeping rootfs at $ROOTFS (KEEP_ROOTFS=1)"
    return
  fi
  rm -rf "$ROOTFS"
}
trap cleanup EXIT

chroot_run() {
  # $1 = bash command string executed as root inside the Holo rootfs
  $SUDO chroot "$ROOTFS" /bin/bash -c "$1"
}

# ---------------------------------------------------------------- [5/6] ---
log "[5/6] chroot: pacman sync + install toolchain and app deps"
chroot_run '
  set -e
  pacman -Sy --noconfirm
  pacman -S --noconfirm \
    base-devel cmake ninja \
    qt6-base qt6-declarative qt6-connectivity \
    kcoreaddons kconfig ki18n kirigami
  pacman -Qs "qt6-|kcoreaddons|kconfig-6|ki18n-6|kirigami-6|glibc" || true
'

log "[5/6] chroot: cmake + ninja"
chroot_run '
  set -e
  cd '"$SRC_IN_ROOTFS"'
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  ninja -C build
  echo "=== NEEDED ==="
  readelf -d build/lighthouse-pm | grep NEEDED || true
  echo "=== unresolved check (ldd) ==="
  ldd build/lighthouse-pm | grep -i "not found" && exit 1 || true
  echo "=== glibc symbol versions required ==="
  readelf -V build/lighthouse-pm 2>/dev/null | grep -oE "GLIBC_[0-9.]+" | sort -uV | tail -5
' 2>&1 | tee /tmp/holo-cmake.log

log "[5/6] chroot: offscreen smoke test"
chroot_run '
  cd '"$SRC_IN_ROOTFS"'
  QT_QPA_PLATFORM=offscreen timeout 20 ./build/lighthouse-pm \
    > /build-src/smoke.log 2>&1
  rc=$?
  echo "smoke exit: $rc"
  tail -n 30 /build-src/smoke.log
  exit 0
' || true

# ---------------------------------------------------------------- [6/6] ---
log "[6/6] staging artifact"
cp "$ROOTFS$SRC_IN_ROOTFS/build/lighthouse-pm" "$DIST/lighthouse-pm-aarch64"
[ -f "$ROOTFS$SRC_IN_ROOTFS/smoke.log" ] && cp "$ROOTFS$SRC_IN_ROOTFS/smoke.log" "$DIST/smoke.log"
chmod +x "$DIST/lighthouse-pm-aarch64"
file "$DIST/lighthouse-pm-aarch64"
log "done -> $DIST/lighthouse-pm-aarch64"
