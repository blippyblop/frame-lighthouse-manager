#!/bin/sh
# Create soname symlinks in the firmware sysroot's /usr/lib.
#
# The extracted rootfs ships only fully-versioned library files
# (libQt6Core.so.6.8.0). The linker and the dynamic loader work with
# sonames (libQt6Core.so.6), so create lib<name>.so.<major> -> the
# versioned file for every library that has a minor/patch version.
#
# Usage: make_fw_symlinks.sh SYSROOT
set -e

LIBDIR=${1:?usage: make_fw_symlinks.sh SYSROOT}/usr/lib
[ -d "$LIBDIR" ] || { echo "missing $LIBDIR"; exit 1; }

count=0
for f in "$LIBDIR"/lib*.so.*; do
    [ -f "$f" ] || continue
    b=$(basename "$f")
    name=${b%%.so.*}
    ver=${b#*.so.}
    major=${ver%%.*}
    soname="$name.so.$major"
    if [ "$soname" != "$b" ] && [ ! -e "$LIBDIR/$soname" ]; then
        ln -s "$b" "$LIBDIR/$soname"
        count=$((count+1))
    fi
done
echo "created $count soname symlinks in $LIBDIR"
