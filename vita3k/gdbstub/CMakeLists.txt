add_library(
gdbstub
STATIC
include/gdbstub/functions.h
include/gdbstub/state.h
src/gdb.cpp)

target_include_directories(gdbstub PUBLIC include)
target_link_libraries(gdbstub PUBLIC cpu host)
