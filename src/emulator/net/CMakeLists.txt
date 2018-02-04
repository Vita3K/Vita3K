add_library(
net
STATIC
include/net/functions.h
include/net/state.h
src/net.cpp
)

target_include_directories(net PUBLIC include)
if (WIN32)
    target_link_libraries(net PUBLIC sdl2 wsock32 ws2_32 vita-headers)
else()
    target_link_libraries(net PUBLIC sdl2 vita-headers)
endif()
