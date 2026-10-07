# CMake toolchain: x86_64 host -> aarch64 glibc Linux (Valve Frame / SteamOS)
#
# Cross compiles with the firmware's own gcc/binutils (aarch64 ELFs wrapped
# for qemu-aarch64 inside the sysroot) so codegen, libstdc++ ABI and the
# linked glibc match the firmware exactly, and links against the firmware
# sysroot's Qt6/KF6/Kirigami headers and libraries.
#
#   FRAME_SYSROOT  (env SYSROOT or -D, default <repo>/build_env/frame-sysroot)
#
# usage:
#   cmake -B build-frame -S . -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-frame.cmake \
#         -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja
#
# The sysroot's own ninja is an aarch64 ELF that the x86_64 host cannot run,
# so the host's ninja must be pinned with -DCMAKE_MAKE_PROGRAM.
#
# The resulting binary is an aarch64 glibc executable for Frame; smoke test
# under 'qemu-aarch64 -L $FRAME_SYSROOT' with the offscreen platform plugin.

if(NOT DEFINED FRAME_SYSROOT OR FRAME_SYSROOT STREQUAL "")
    set(FRAME_SYSROOT "$ENV{SYSROOT}")
endif()
if(NOT FRAME_SYSROOT)
    set(FRAME_SYSROOT "${CMAKE_CURRENT_LIST_DIR}/../build_env/frame-sysroot")
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_CROSSCOMPILING TRUE)

set(CMAKE_C_COMPILER   "${FRAME_SYSROOT}/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "${FRAME_SYSROOT}/usr/bin/g++")

set(CMAKE_SYSROOT "${FRAME_SYSROOT}")
set(CMAKE_C_FLAGS_INIT   "--sysroot=${FRAME_SYSROOT}")
set(CMAKE_CXX_FLAGS_INIT "--sysroot=${FRAME_SYSROOT}")
set(CMAKE_EXE_LINKER_FLAGS_INIT   "--sysroot=${FRAME_SYSROOT}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "--sysroot=${FRAME_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH "${FRAME_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# PROGRAM stays BOTH: the firmware Qt tools (moc/rcc/qmlcachegen/...) are
# absolute paths inside the sysroot and are already qemu-wrapped.

set(CMAKE_CROSSCOMPILING_EMULATOR "qemu-aarch64;-L;${FRAME_SYSROOT}")

set(CMAKE_PREFIX_PATH "${FRAME_SYSROOT}/usr" CACHE STRING "" FORCE)