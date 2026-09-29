set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)

# Optimization and CPU flags; --target is also passed via CMAKE_*_COMPILER_TARGET
# in the cmake/caches/circle-newlib-aarch64-none-circle.cmake cache file.
set(FLAGS "${CIRCLE_ARCHCPU} -ffreestanding -mstrict-align --target=aarch64-none-elf")

set(CMAKE_C_FLAGS "${FLAGS}" CACHE STRING "")
set(CMAKE_CXX_FLAGS "${FLAGS}" CACHE STRING "")
set(CMAKE_ASM_FLAGS "${FLAGS}" CACHE STRING "")

# Tell CMake not to try to link a dummy executable during tests
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
