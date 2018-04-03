add_library(
host
STATIC
include/host/functions.h
include/host/import_fn.h
include/host/rtc.h
include/host/state.h
include/host/version.h
src/host.cpp
src/rtc.cpp
version.cpp
)

configure_file(src/version.cpp.in version.cpp)

target_include_directories(host PUBLIC include)
target_link_libraries(host PUBLIC audio cpu ctrl gxm io kernel mem net util)
target_link_libraries(host PRIVATE glutil nids sdl2)
