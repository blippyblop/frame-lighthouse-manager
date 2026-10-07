# Cross-toolchain for the Valve Frame: uses the firmware's own gcc 15.1.1 /
# binutils (wrapped for qemu-aarch64 in build_env/frame-sysroot) so codegen,
# libstdc++ ABI and linker output match the firmware exactly.
#
#   SYSROOT   (env, default <this dir>/frame-sysroot)
#
# usage: cmake -B build-frame -S . -G Ninja \
#            -DCMAKE_TOOLCHAIN_FILE=build_env/toolchain_frame.cmake
set(SYSROOT "$ENV{SYSROOT}")
if(SYSROOT STREQUAL "")
    get_filename_component(SYSROOT "${CMAKE_CURRENT_LIST_DIR}/frame-sysroot" ABSOLUTE)
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_CROSSCOMPILING TRUE)

set(CMAKE_C_COMPILER   "${SYSROOT}/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "${SYSROOT}/usr/bin/g++")

set(CMAKE_SYSROOT "${SYSROOT}")
set(CMAKE_C_FLAGS_INIT   "--sysroot=${SYSROOT}")
set(CMAKE_CXX_FLAGS_INIT "--sysroot=${SYSROOT}")
set(CMAKE_EXE_LINKER_FLAGS_INIT   "--sysroot=${SYSROOT}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "--sysroot=${SYSROOT}")

set(CMAKE_FIND_ROOT_PATH "${SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# PROGRAM stays NONE: the firmware Qt tools (moc/rcc/qmlimportscanner/...) are
# absolute paths inside the sysroot and are already qemu-wrapped.

set(CMAKE_CROSSCOMPILING_EMULATOR "qemu-aarch64;-L;${SYSROOT}")

set(CMAKE_PREFIX_PATH "${SYSROOT}/usr" CACHE STRING "" FORCE)
