add_library(
gui
STATIC
include/gui/imgui_impl_sdl_gl3.h
include/gui/functions.h
include/gui/state.h
src/imgui_impl_sdl_gl3.cpp
src/ui_common_dialog.cpp
src/ui_condvars_dialog.cpp
src/ui_eventflags_dialog.cpp
src/ui_main.cpp
src/ui_main_menubar.cpp
src/ui_mutexes_dialog.cpp
src/ui_semaphores_dialog.cpp
src/ui_threads_dialog.cpp
)

target_include_directories(gui PUBLIC include)
target_link_libraries(gui PUBLIC sdl2 imgui glutil host kernel util dialog)
