add_library(
    vita_shell
    STATIC
    include/vita_shell/functions.h
    include/vita_shell/state.h
    src/themes.cpp
)

target_include_directories(vita_shell PUBLIC include)
target_link_libraries(vita_shell PUBLIC util PRIVATE pugixml::pugixml)