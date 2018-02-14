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
target_link_libraries(host PUBLIC audio cpu ctrl gxm io kernel mem net spdlog util)
target_link_libraries(host PRIVATE nids sdl2)
