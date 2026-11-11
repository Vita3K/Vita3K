add_library(
    vita_shell
    STATIC
    include/vita_shell/functions.h
    include/vita_shell/state.h
    src/theme_catalog.cpp
    src/themes.cpp
)

target_include_directories(vita_shell PUBLIC include)
target_link_libraries(vita_shell PUBLIC util io vfs PRIVATE regmgr pugixml::pugixml)
