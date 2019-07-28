add_library(
	gui
	STATIC
	include/gui/functions.h
	include/gui/imgui_impl_sdl_gl3.h
	include/gui/state.h
	src/common_dialog.cpp
	src/condvars_dialog.cpp
	src/eventflags_dialog.cpp
	src/game_selector.cpp
	src/gui.cpp
	src/imgui_impl_sdl_gl3.cpp
	src/main_menubar.cpp
	src/mutexes_dialog.cpp
	src/private.h
	src/reinstall.cpp
	src/semaphores_dialog.cpp
	src/settings_dialog.cpp
	src/threads_dialog.cpp
	src/controls_dialog.cpp
	src/allocations_dialog.cpp
	src/disassembly_dialog.cpp
	src/about_dialog.cpp
)

target_include_directories(gui PUBLIC include)
target_link_libraries(gui PUBLIC host imgui)
target_link_libraries(gui PRIVATE stb)
