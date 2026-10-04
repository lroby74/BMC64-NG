set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)

# Optimization and CPU flags
set(FLAGS "${CIRCLE_ARCHCPU} -ffreestanding -mno-unaligned-access -DAARCH=32 --target=arm-none-eabi")

set(CMAKE_C_FLAGS "${FLAGS}" CACHE STRING "")
set(CMAKE_CXX_FLAGS "${FLAGS}" CACHE STRING "")
set(CMAKE_ASM_FLAGS "${FLAGS}" CACHE STRING "")

# Tell CMake not to try to link a dummy executable during tests
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
