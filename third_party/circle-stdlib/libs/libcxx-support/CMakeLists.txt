cmake_minimum_required(VERSION 3.14)
project(libcxx-support CXX)

set(CIRCLE_INCLUDE_DIR "" CACHE PATH
    "Path to Circle include directory (libs/circle/include)")
set(LIBCXX_INCLUDE_DIR "" CACHE PATH
    "Path to installed libc++ include directory (install/<arch>-libc++/include/c++/v1)")
set(NEWLIB_INCLUDE_DIR "" CACHE PATH
    "Path to installed newlib include directory (install/<arch>/include)")
set(LIBCXX_THREADING_INCLUDE_DIR "" CACHE PATH
    "Path to libcxx-threading include directory (libs/libcxx-threading/include)")

add_library(libcxx-support STATIC
    src/string.cpp
    src/wstring.cpp
)

target_include_directories(libcxx-support SYSTEM PRIVATE
    ${CIRCLE_INCLUDE_DIR}
    ${LIBCXX_INCLUDE_DIR}
    ${NEWLIB_INCLUDE_DIR}
    ${LIBCXX_THREADING_INCLUDE_DIR}
)

target_compile_features(libcxx-support PRIVATE cxx_std_14)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(libcxx-support PRIVATE -Wno-alloc-size-larger-than)
endif()

target_compile_definitions(libcxx-support PRIVATE _POSIX_C_SOURCE=200809L)

include(GNUInstallDirs)

install(TARGETS libcxx-support
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Usage example:
#   cmake \
#     -DCIRCLE_INCLUDE_DIR=/path/to/circle-stdlib/libs/circle/include \
#     -DLIBCXX_INCLUDE_DIR=/path/to/circle-stdlib/install/arm-none-circle-libc++/include/c++/v1 \
#     -DNEWLIB_INCLUDE_DIR=/path/to/circle-stdlib/install/arm-none-circle/include \
#     -DLIBCXX_THREADING_INCLUDE_DIR=/path/to/circle-stdlib/libs/libcxx-threading/include \
#     -DCMAKE_INSTALL_PREFIX=/path/to/circle-stdlib/install/arm-none-circle-libc++ \
#     -DCMAKE_TOOLCHAIN_FILE=/path/to/circle-stdlib/cmake/toolchains/toolchain-arm-none-circle.cmake \
#     /path/to/libs/libcxx-support
#
#   cmake --build .
#   cmake --install .
#
# Installs:
#   ${CMAKE_INSTALL_PREFIX}/lib/liblibcxx-support.a
