cmake_minimum_required(VERSION 3.14)
project(libcxx-threading CXX)

set(CIRCLE_INCLUDE_DIR "" CACHE PATH
    "Path to Circle include directory (libs/circle/include)")
set(LIBCXX_INCLUDE_DIR "" CACHE PATH
    "Path to installed libc++ include directory (install/<arch>-libc++/include/c++/v1)")
set(NEWLIB_INCLUDE_DIR "" CACHE PATH
    "Path to installed newlib include directory (install/<arch>/include)")

add_library(libcxx-threading STATIC
    src/condvar.cpp
    src/mutex.cpp
    src/thread.cpp
)

target_include_directories(libcxx-threading
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(libcxx-threading SYSTEM PRIVATE
    ${CIRCLE_INCLUDE_DIR}
    ${LIBCXX_INCLUDE_DIR}
    ${NEWLIB_INCLUDE_DIR}
)

target_compile_features(libcxx-threading PRIVATE cxx_std_14)

target_compile_definitions(libcxx-threading PRIVATE _POSIX_C_SOURCE=200809L)

include(GNUInstallDirs)

install(TARGETS libcxx-threading
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/__external_threading
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Usage example:
#   cmake \
#     -DCIRCLE_INCLUDE_DIR=/path/to/circle-stdlib/libs/circle/include \
#     -DLIBCXX_INCLUDE_DIR=/path/to/circle-stdlib/install/aarch64-none-circle-libc++/include/c++/v1 \
#     -DNEWLIB_INCLUDE_DIR=/path/to/circle-stdlib/install/aarch64-none-circle/include \
#     -DCMAKE_INSTALL_PREFIX=/path/to/install \
#     -DCMAKE_TOOLCHAIN_FILE=/path/to/circle-stdlib/cmake/toolchains/toolchain-aarch64-none-circle.cmake \
#     /path/to/libs/libcxx-threading
#
#   cmake --build .
#   cmake --install .
#
# Installs:
#   ${CMAKE_INSTALL_PREFIX}/lib/liblibcxx-threading.a
#   ${CMAKE_INSTALL_PREFIX}/include/__external_threading
