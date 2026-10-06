# CMake toolchain: x86_64 Alpine host -> aarch64 musl Linux (KDE)
#
# Cross compiles with the native x86_64 'aarch64-none-elf' gcc toolchain,
# linking against the musl libraries and Qt6/KF6/Kirigami headers and libs
# extracted into the sysroot at $ENV{LHPM_SYSROOT} (default /opt/aarch64-sysroot).
#
# The resulting binaries are aarch64 ELF executables that run on any
# aarch64 musl Linux (e.g. Alpine/KDE) and can be smoke-tested under
# 'qemu-aarch64 -L $SYSROOT'.

if(NOT DEFINED CROSS_SYSROOT)
    set(CROSS_SYSROOT "/opt/aarch64-sysroot")
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_C_COMPILER "aarch64-none-elf-gcc")
set(CMAKE_CXX_COMPILER "aarch64-none-elf-g++")

set(CMAKE_FIND_ROOT_PATH ${CROSS_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAMS BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# libstdc++ from the aarch64-none-elf toolchain uses #include_next for C
# headers, which skips isystem dirs and would pick up newlib's C headers;
# those are renamed aside and musl's are offered via -idirafter instead.
set(CMAKE_C_FLAGS_INIT "-D__linux__ -D_GNU_SOURCE -O2 -idirafter ${CROSS_SYSROOT}/usr/include -include musl-ctype-shim.h")
set(CMAKE_CXX_FLAGS_INIT "-D__linux__ -D_GNU_SOURCE -O2 -idirafter ${CROSS_SYSROOT}/usr/include -include musl-ctype-shim.h")

# musl CRT objects (crt1/crtn/crti) plus GCC's crtbegin/crtend for
# __dso_handle, and musl's dynamic loader.
file(GLOB _gccver_dirs RELATIVE "/usr/lib/gcc/aarch64-none-elf" "/usr/lib/gcc/aarch64-none-elf/*")
list(SORT _gccver_dirs)
list(GET _gccver_dirs -1 _gccver)
set(CROSS_GCC_LIBDIR "/usr/lib/gcc/aarch64-none-elf/${_gccver}")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-nostartfiles ${CROSS_SYSROOT}/usr/lib/crti.o ${CROSS_GCC_LIBDIR}/crtbegin.o ${CROSS_SYSROOT}/usr/lib/crt1.o -Wl,--dynamic-linker=/lib/ld-musl-aarch64.so.1")

set(CMAKE_SYSROOT "${CROSS_SYSROOT}")

# The aarch64-none-elf target has no default library path, so the musl C
# runtime and C++ standard library are linked explicitly (shared, as a KDE
# system would provide them). crtend/crtn close the startup files.
set(CMAKE_C_STANDARD_LIBRARIES "-L${CROSS_SYSROOT}/usr/lib -lm -lc ${CROSS_GCC_LIBDIR}/crtend.o ${CROSS_SYSROOT}/usr/lib/crtn.o")
set(CMAKE_CXX_STANDARD_LIBRARIES "-L${CROSS_SYSROOT}/usr/lib -lstdc++ -lm -lc ${CROSS_GCC_LIBDIR}/crtend.o ${CROSS_SYSROOT}/usr/lib/crtn.o")

# The sysroot is the only source of libraries at link time.
set(CMAKE_LIBRARY_ARCHITECTURE aarch64)

# Prefer our local FindOpenMP.cmake (next to this toolchain): the stock
# module runs compile tests that the --disable-threads cross compiler fails.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

# Qt6 tools (moc/uic/rcc) are aarch64 binaries; the sysroot copies are
# wrapper scripts that run the real binaries through qemu.
set(Qt6Core_MOC_EXECUTABLE "${CROSS_SYSROOT}/usr/lib/qt6/libexec/moc")
set(Qt6Core_UIC_EXECUTABLE "${CROSS_SYSROOT}/usr/lib/qt6/libexec/uic")
set(Qt6Core_RCC_EXECUTABLE "${CROSS_SYSROOT}/usr/lib/qt6/libexec/rcc")

# QML import path for qmlcachegen (Kirigami etc. live in the sysroot).
set(QT_QML_IMPORT_PATH "${CROSS_SYSROOT}/usr/lib/qt6/qml")

set(CMAKE_PREFIX_PATH_INIT "${CROSS_SYSROOT}/usr/lib/cmake;${CROSS_SYSROOT}/usr")
