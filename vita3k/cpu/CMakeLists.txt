set(SOURCE_LIST
include/cpu/state.h
include/cpu/common.h
include/cpu/functions.h
include/cpu/impl/interface.h
include/cpu/disasm/functions.h
include/cpu/disasm/state.h

src/disasm.cpp
src/cpu.cpp
)

if(DYNARMIC_ENABLED)
    list(APPEND SOURCE_LIST
        include/cpu/impl/dynarmic_cpu.h
        src/dynarmic_cpu.cpp
    )
endif()

add_library(
cpu
STATIC
${SOURCE_LIST}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_LIST})

target_include_directories(cpu PUBLIC include)
target_link_libraries(cpu PUBLIC mem util)

set(CPU_LINK_LIBRARIES capstone merry::mcl)
if(DYNARMIC_ENABLED)
    list(APPEND CPU_LINK_LIBRARIES dynarmic)
endif()

target_link_libraries(cpu PRIVATE ${CPU_LINK_LIBRARIES})
