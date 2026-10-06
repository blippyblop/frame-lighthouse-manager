#!/bin/sh
# Wrap the aarch64 Qt build tools inside the sysroot with qemu-aarch64 so
# the x86_64 cross host can execute them during the build (moc, uic, rcc,
# qmlcachegen, cmake_automoc_parser, ...).
#
# Usage: wrap_sysroot_qemu.sh [SYSROOT]   (default /opt/aarch64-sysroot)
set -e

SYSROOT=${1:-/opt/aarch64-sysroot}
[ -d "$SYSROOT/usr/lib/qt6" ] || { echo "sysroot missing: $SYSROOT"; exit 1; }
command -v qemu-aarch64 >/dev/null || { echo "qemu-aarch64 not found (apk add qemu-aarch64)"; exit 1; }

wrap() {
    f="$1"
    [ -f "$f" ] || return 0
    [ -f "$f.real" ] && return 0
    # only aarch64 ELF files (e_machine == 0xb7, little-endian at offset 18)
    [ "$(head -c 4 "$f" | od -An -tx1 | tr -d ' ')" = "7f454c46" ] || return 0
    machine=$(head -c 20 "$f" | tail -c 2 | od -An -tx1 | tr -d ' ')
    [ "$machine" = "b700" ] || return 0
    mv "$f" "$f.real"
    printf '#!/bin/sh\nexec qemu-aarch64 -L %s %s.real "$@"\n' "$SYSROOT" "$f" > "$f"
    chmod +x "$f"
    echo "wrapped $f"
}

for d in usr/lib/qt6/libexec usr/lib/qt6/bin; do
    for f in "$SYSROOT/$d"/*; do
        wrap "$f"
    done
done

echo "sysroot Qt tools wrapped for qemu"
