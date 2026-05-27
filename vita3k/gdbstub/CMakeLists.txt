add_library(
    gdbstub
    STATIC
    include/gdbstub/functions.h
    include/gdbstub/state.h
    src/gdb.cpp
)

target_include_directories(gdbstub PUBLIC include)
target_link_libraries(gdbstub PUBLIC cpu emuenv)
target_link_libraries(gdbstub PRIVATE audio config io kernel)

if(NOT ANDROID)
    add_executable(
        gdbstub-tests
        tests/gdb_state_tests.cpp
    )

    target_include_directories(gdbstub-tests PRIVATE include)
    target_link_libraries(gdbstub-tests PRIVATE gdbstub googletest)
    add_test(NAME gdbstub COMMAND gdbstub-tests)
endif()
