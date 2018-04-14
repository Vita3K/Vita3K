add_library(
gui
STATIC
include/gui/imgui_impl_sdl_gl3.h
src/imgui_impl_sdl_gl3.cpp
)

target_include_directories(gui PUBLIC include)
target_link_libraries(gui PUBLIC sdl2 imgui glutil)
