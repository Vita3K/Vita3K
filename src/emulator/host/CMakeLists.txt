add_library(
host
STATIC
include/host/functions.h
include/host/import_fn.h
include/host/state.h
include/host/version.h
src/host.cpp
version.cpp
)

configure_file(src/version.cpp.in version.cpp)

target_include_directories(host PUBLIC include)
target_link_libraries(host PUBLIC audio cpu ctrl gxm io kernel mem net)
if (WIN32)
    target_link_libraries(host PRIVATE nids sdl2 wsock32 ws2_32)
else()
    target_link_libraries(host PRIVATE nids sdl2)
endif()

