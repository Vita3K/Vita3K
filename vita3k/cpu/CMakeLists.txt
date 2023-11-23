set(SOURCE_LIST
include/cpu/state.h
include/cpu/common.h
include/cpu/functions.h
include/cpu/impl/dynarmic_cpu.h
include/cpu/impl/interface.h
include/cpu/disasm/functions.h
include/cpu/disasm/state.h

src/disasm.cpp
src/cpu.cpp
src/dynarmic_cpu.cpp
)

if(USE_UNICORN)
    list(add SOURCE_LIST include/cpu/impl/unicorn_cpu.h src/unicorn_cpu.cpp)
endif()

add_library(
cpu
STATIC
${SOURCE_LIST}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_LIST})

target_include_directories(cpu PUBLIC include)
target_link_libraries(cpu PUBLIC mem util)
target_link_libraries(cpu PRIVATE dynarmic capstone merry::mcl)
if(USE_UNICORN)
    target_link_libraries(cpu PRIVATE unicorn)
    target_compile_definitions(cpu PRIVATE USE_UNICORN)
endif()