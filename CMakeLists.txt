cmake_minimum_required(VERSION 3.10)
project(Vita3K)

enable_testing()

add_subdirectory(src/external)
add_subdirectory(src/emulator)
add_subdirectory(src/gen-modules)
