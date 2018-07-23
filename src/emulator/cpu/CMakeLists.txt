add_library(
cpu
STATIC
include/cpu/functions.h
include/cpu/dynarmic_cpu.h
include/cpu/unicorn_cpu.h
src/cpu.cpp
src/dynarmic_cpu.cpp
src/unicorn_cpu.cpp
)

target_include_directories(cpu PUBLIC include)
target_link_libraries(cpu PUBLIC disasm mem)
target_link_libraries(cpu PRIVATE unicorn dynarmic)
