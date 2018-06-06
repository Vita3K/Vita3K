cmake_minimum_required(VERSION 3.10)
project(Vita3K)

enable_testing()

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_subdirectory(src/external)
add_subdirectory(src/emulator)
add_subdirectory(src/gen-modules)
