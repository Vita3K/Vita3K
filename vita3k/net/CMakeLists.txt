add_library(
net
STATIC
include/net/functions.h
include/net/state.h
include/net/types.h
src/net.cpp
)

target_include_directories(net PUBLIC include)
target_link_libraries(net PUBLIC mem util)
if (WIN32)
    target_link_libraries(net PRIVATE winsock)
endif()
