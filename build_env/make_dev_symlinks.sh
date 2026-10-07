#!/bin/bash
# Create dev symlinks (libX.so -> libX.so.MAJOR) in the sysroot copy so the
# firmware's ld can resolve -lX during our build. Only the build sysroot is
# touched; the device runtime uses the soname chain.
#   usage: make_dev_symlinks.sh SYSROOT
set -euo pipefail
R="${1:?usage: make_dev_symlinks.sh SYSROOT}"
[ -d "$R/usr/lib" ] || { echo "no $R/usr/lib" >&2; exit 1; }
L="$R/usr/lib"

declare -A best bestn
for f in "$L"/lib*.so.*; do
    [ -e "$f" ] || continue
    name="${f##*/}"
    case "$name" in
        lib*.so.[0-9]*) : ;;
        *) continue ;;
    esac
    base="${name%%.so.*}.so"
    ver="${name##*.so.}"
    case "$ver" in
        *[!0-9.]*) continue ;;
    esac
    ncomp=$(tr -cd '.' <<< "$ver" | wc -c)
    if [ -z "${best[$base]+x}" ] || [ "$ncomp" -lt "${bestn[$base]}" ]; then
        best[$base]="$name"
        bestn[$base]="$ncomp"
    fi
done

count=0
for base in "${!best[@]}"; do
    [ -e "$L/$base" ] && continue
    ln -s "${best[$base]}" "$L/$base"
    count=$((count+1))
done
echo "created $count dev symlinks in $L"
