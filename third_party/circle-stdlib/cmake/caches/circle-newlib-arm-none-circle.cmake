include("${CMAKE_CURRENT_LIST_DIR}/circle-newlib-common.cmake")

set(CMAKE_ASM_COMPILER_TARGET "arm-none-eabi" CACHE STRING "")
set(CMAKE_C_COMPILER_TARGET   "arm-none-eabi" CACHE STRING "")
set(CMAKE_CXX_COMPILER_TARGET "arm-none-eabi" CACHE STRING "")

# arm/divsf3.S uses the 'mls' instruction (ARMv6T2+), which is not available
# on the ARM1176JZF-S (ARMv6) used by Raspberry Pi 1. Use the C fallback.
set(COMPILER_RT_ARM_OPTIMIZED_FP OFF CACHE BOOL "")
