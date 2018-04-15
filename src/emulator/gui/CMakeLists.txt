add_library(
gui
STATIC
include/gui/imgui_impl_sdl_gl2.h
include/gui/functions.h
include/gui/state.h
src/imgui_impl_sdl_gl2.cpp
src/ui_main.cpp
src/ui_main_menubar.cpp
src/ui_threads_dialog.cpp
)

target_include_directories(gui PUBLIC include)
target_link_libraries(gui PUBLIC sdl2 imgui glutil host kernel util)
