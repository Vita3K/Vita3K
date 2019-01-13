add_library(
cpu
STATIC
include/cpu/common.h
include/cpu/dynarmic_cpu.h
include/cpu/factory.h
include/cpu/functions.h
include/cpu/interface.h
include/cpu/state.h
include/cpu/unicorn_cpu.h
src/cpu.cpp
src/dynarmic_cpu.cpp
src/factory.cpp
src/unicorn_cpu.cpp
)

target_include_directories(cpu PUBLIC include)
target_link_libraries(cpu PUBLIC disasm mem util)
target_link_libraries(cpu PRIVATE  dynarmic unicorn)
