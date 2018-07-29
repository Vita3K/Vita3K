cmake_minimum_required(VERSION 3.10)
project(Vita3K)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

enable_testing()

add_subdirectory(src/external)
add_subdirectory(src/emulator)
add_subdirectory(src/gen-modules)
