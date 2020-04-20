add_library(
cpu
STATIC
include/cpu/functions.h
src/cpu.cpp
)

target_include_directories(cpu PUBLIC include)
target_link_libraries(cpu PUBLIC disasm mem)
target_link_libraries(cpu PRIVATE unicorn)
