#!/bin/bash
# Wrap the firmware's own aarch64 gcc/binutils with a qemu-aarch64 shim so the
# driver (aarch64 ELF) can be exec'd from x86. The wrapper is a text script;
# it execs qemu with -L $SYSROOT (sysroot has usrmerge symlinks so the ELF
# interpreter /lib/ld-linux-aarch64.so.1 resolves).
#   usage: wrap_fw_compiler.sh SYSROOT
set -euo pipefail

R="${1:?usage: wrap_fw_compiler.sh SYSROOT}"
[ -d "$R" ] || { echo "no such dir: $R" >&2; exit 1; }
QEMU=/usr/bin/qemu-aarch64

# usrmerge symlinks (device layout; extraction dropped them)
ln -sfn usr/bin   "$R/bin"
ln -sfn usr/sbin  "$R/sbin"
ln -sfn usr/lib   "$R/lib"
ln -sfn usr/lib   "$R/lib64"
ln -sfn usr/lib32 "$R/lib32" 2>/dev/null || ln -sfn usr/lib "$R/lib32"
ln -sfn usr/lib   "$R/libx32"
ln -sfn usr/include "$R/include"

is_aarch64_elf() {
    local m
    m=$(od -An -tx1 -N4 "$1" | tr -d ' ')
    [ "$m" = "7f454c46" ] || return 1
    local em
    em=$(od -An -j18 -N2 -tx1 "$1" | tr -d ' ')
    [ "$em" = "b700" ]
}

wrap() {
    local f="$1"
    [ -f "$f" ] || return 0
    [ -f "${f}.real" ] && return 0
    if is_aarch64_elf "$f"; then
        chmod +x "$f"
        mv "$f" "${f}.real"
        cat > "$f" <<EOF
#!/bin/sh
exec $QEMU -L $R ${f}.real "\$@"
EOF
        chmod +x "$f"
        echo "wrapped: $f"
    fi
}

# gcc drivers + cpp
for t in gcc g++ cc c++ cpp; do wrap "$R/usr/bin/$t"; done
# binutils used by the driver
for t in as ld ld.bfd ld.lld ar nm ranlib strip objcopy objdump; do wrap "$R/usr/bin/$t"; done
# gcc internals (cc1/cc1plus/collect2/lto1/lto-wrapper)
for d in "$R/usr/lib/gcc/aarch64-unknown-linux-gnu/15.1.1" "$R/usr/libexec/gcc/aarch64-unknown-linux-gnu/15.1.1"; do
    [ -d "$d" ] || continue
    for t in cc1 cc1plus collect2 lto1 lto-wrapper; do wrap "$d/$t"; done
done

echo "done"
