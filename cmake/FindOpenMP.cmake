# Minimal FindOpenMP replacement for aarch64 cross builds.
#
# The aarch64-none-elf GCC is built with --disable-threads, so the stock
# FindOpenMP compile tests fail (the C++ driver adds -pthread, which that
# target rejects). Kirigami ships pre-built with OpenMP support, so we only
# need find_package(OpenMP) to succeed here — no actual compile tests.

set(OpenMP_C_FLAGS "-fopenmp" CACHE STRING "OpenMP C flags")
set(OpenMP_CXX_FLAGS "-fopenmp" CACHE STRING "OpenMP CXX flags")
set(OpenMP_C_LIB_NAMES "gomp")
set(OpenMP_CXX_LIB_NAMES "gomp")
set(OpenMP_C_FOUND TRUE)
set(OpenMP_CXX_FOUND TRUE)
set(OpenMP_FOUND TRUE)
