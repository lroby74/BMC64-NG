set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-none-elf-gcc)
set(CMAKE_CXX_COMPILER aarch64-none-elf-g++)
set(CMAKE_ASM_COMPILER aarch64-none-elf-gcc)

# Optimization and CPU flags
set(FLAGS "${CIRCLE_ARCHCPU} -ffreestanding -mstrict-align")

set(CMAKE_C_FLAGS "${FLAGS}" CACHE STRING "")
set(CMAKE_CXX_FLAGS "${FLAGS}" CACHE STRING "")

# Tell CMake not to try to link a dummy executable during tests
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

