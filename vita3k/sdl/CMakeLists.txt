add_library(
    sdl
    STATIC
    include/sdl/frame_host.h
    include/sdl/functions.h
    include/sdl/state.h
    src/console.cpp
    src/event.cpp
    src/frame_host.cpp
    src/init.cpp
    src/window.cpp
)

target_include_directories(sdl PUBLIC include)
target_include_directories(sdl PRIVATE ${CMAKE_SOURCE_DIR}/vita3k)
target_link_libraries(sdl PUBLIC SDL3::SDL3 input renderer)
target_link_libraries(sdl PRIVATE app config ctrl display emuenv ime io motion packages psvpfsparser touch util)
