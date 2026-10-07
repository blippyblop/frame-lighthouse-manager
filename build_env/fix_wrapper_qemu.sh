#!/bin/bash
# Repoint all in-place qemu wrappers to a qemu-aarch64 COPY inside the sysroot's
# usr/bin, so the firmware gcc driver (running under that copy) resolves
# /proc/self/exe to <sysroot>/usr/bin and finds the firmware as/ld/cc1plus
# instead of the host's x86 toolchain.
#   usage: fix_wrapper_qemu.sh SYSROOT
set -euo pipefail
R="${1:?usage: fix_wrapper_qemu.sh SYSROOT}"
[ -d "$R" ] || { echo "no such dir: $R" >&2; exit 1; }

cp -f /usr/bin/qemu-aarch64 "$R/usr/bin/qemu-aarch64"
chmod +x "$R/usr/bin/qemu-aarch64"

count=0
while IFS= read -r f; do
    case "$f" in
        *.real) continue ;;
    esac
    if head -1 "$f" 2>/dev/null | grep -q '^#!.*sh' && grep -q 'qemu-aarch64 -L' "$f" 2>/dev/null; then
        sed -i "s#[^ ]*qemu-aarch64 -L#$R/usr/bin/qemu-aarch64 -L#" "$f"
        count=$((count+1))
    fi
done < <(find "$R/usr/bin" "$R/usr/lib/qt6" "$R/usr/lib/gcc" "$R/usr/bin" -maxdepth 4 -type f 2>/dev/null)
echo "repointed $count wrapper(s) to $R/usr/bin/qemu-aarch64"
