#!/bin/sh
# Make the aarch64-none-elf cross toolchain use the musl (sysroot) C
# headers instead of newlib's. libstdc++'s #include_next <stdlib.h> chain
# skips -isystem directories and would otherwise pick up newlib's C
# headers; the newlib top-level headers are moved aside and musl's are
# symlinked into the toolchain include dir instead.
#
# Usage: fix_cross_headers.sh [SYSROOT]   (default /opt/aarch64-sysroot)
set -e

SYSROOT=${1:-/opt/aarch64-sysroot}
TOOLCHAIN_INC=/usr/aarch64-none-elf/include
BACKUP=/opt/newlib-includes-backup/newlib-disabled

[ -d "$SYSROOT/usr/include" ] || { echo "sysroot missing: $SYSROOT"; exit 1; }
[ -d "$TOOLCHAIN_INC" ] || { echo "toolchain include dir missing: $TOOLCHAIN_INC (apk add newlib-aarch64-none-elf)"; exit 1; }

# install the ctype shim into the sysroot include dir
cp "$(dirname "$0")/musl-ctype-shim.h" "$SYSROOT/usr/include/musl-ctype-shim.h"

mkdir -p "$BACKUP"
# move newlib's top-level headers aside (keep c++/ which holds libstdc++)
for e in "$TOOLCHAIN_INC"/*; do
    [ -e "$e" ] || continue
    name=$(basename "$e")
    [ "$name" = "c++" ] && continue
    [ -e "$BACKUP/$name" ] && continue
    mv "$e" "$BACKUP/$name"
done

# offer musl's headers instead
for e in "$SYSROOT"/usr/include/*; do
    [ -e "$e" ] || continue
    name=$(basename "$e")
    ln -sfn "$e" "$TOOLCHAIN_INC/$name"
done

echo "cross headers fixed (newlib -> $BACKUP, musl -> $TOOLCHAIN_INC)"
