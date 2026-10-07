#!/bin/sh
# Make the aarch64-none-elf cross toolchain use the SteamOS firmware's
# glibc C headers instead of newlib's.
#
# The newlib g++ driver resolves its C headers from its own include dir,
# and libstdc++'s #include_next <...> chains land there. By replacing
# that dir's top-level entries with symlinks into the firmware sysroot's
# /usr/include, every C header resolves to the firmware's glibc headers
# while the C++ standard library headers (c++/16.1.0) stay as-is.
#
# Usage: setup_frame_includes.sh SYSROOT
set -e

SYSROOT=${1:?usage: setup_frame_includes.sh SYSROOT}
INC=/usr/aarch64-none-elf/include
FW_INC="$SYSROOT/usr/include"

[ -d "$FW_INC" ] || { echo "firmware include dir missing: $FW_INC"; exit 1; }

# Save the C++ standard library tree (only toolchain-local content we keep).
CPP_TREE=$(mktemp -d /tmp/gcc-cpp-tree.XXXXXX)
if [ -d "$INC/c++" ]; then
    cp -a "$INC/c++" "$CPP_TREE/c++"
fi

rm -rf "$INC"
mkdir -p "$INC"
if [ -d "$CPP_TREE/c++" ]; then
    cp -a "$CPP_TREE/c++" "$INC/c++"
    rm -rf "$CPP_TREE"
fi

# Symlink every firmware header into the toolchain include dir.
for f in "$FW_INC"/*; do
    ln -sfn "$f" "$INC/$(basename "$f")"
done
echo "toolchain include dir now points at firmware headers: $FW_INC"
