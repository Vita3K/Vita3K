if (USE_VULKAN)
	set(IMGUI_IMPL_VULKAN_SOURCES
		include/gui/imgui_impl_sdl_vulkan.h

		src/imgui_impl_sdl_vulkan.cpp
	)
else()
	set(IMGUI_IMPL_VULKAN_SOURCES "")
endif()

add_library(
	gui
	STATIC
	include/gui/functions.h
	include/gui/imgui_impl_sdl_gl3.h
	include/gui/imgui_impl_sdl_state.h
	include/gui/imgui_impl_sdl.h
	include/gui/state.h
	src/common_dialog.cpp
	src/condvars_dialog.cpp
	src/eventflags_dialog.cpp
	src/firmware_install_dialog.cpp
	src/game_context_menu.cpp
	src/game_install_dialog.cpp
	src/game_selector.cpp
	src/gui.cpp
	src/imgui_impl_sdl_gl3.cpp
	${IMGUI_IMPL_VULKAN_SOURCES}
	src/imgui_impl_sdl.cpp
	src/live_area_dialog.cpp
	src/main_menubar.cpp
	src/manual_dialog.cpp
	src/mutexes_dialog.cpp
	src/perf_overlay.cpp
	src/pkg_install_dialog.cpp
	src/private.h
	src/profiles_manager_dialog.cpp
	src/reinstall.cpp
	src/semaphores_dialog.cpp
	src/settings_dialog.cpp
	src/threads_dialog.cpp
	src/controls_dialog.cpp
	src/allocations_dialog.cpp
	src/disassembly_dialog.cpp
	src/about_dialog.cpp
	src/trophy_unlocked.cpp
)

target_include_directories(gui PUBLIC include ${CMAKE_SOURCE_DIR}/vita3k)
target_link_libraries(gui PUBLIC app host imgui glutil)
target_link_libraries(gui PRIVATE nativefiledialog pugixml stb renderer)
