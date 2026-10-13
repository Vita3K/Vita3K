set(GUI_SOURCES
	include/gui-imgui/functions.h
	include/gui-imgui/imgui_impl_sdl_gl3.h
	include/gui-imgui/imgui_impl_sdl_state.h
	include/gui-imgui/imgui_impl_sdl_vulkan.h
	include/gui-imgui/imgui_impl_sdl.h
	include/gui-imgui/state.h
	include/gui-imgui/lang_state.h
	include/gui-imgui/popups.h
	src/app_context_menu.cpp
	src/archive_install_dialog.cpp
	src/condvars_dialog.cpp
	src/compat.cpp
	src/content_manager.cpp
	src/eventflags_dialog.cpp
	src/firmware_install_dialog.cpp
	src/gui.cpp
	src/lang.cpp
	src/home_screen.cpp
	src/ime.cpp
	src/imgui_impl_sdl_gl3.cpp
	src/imgui_impl_sdl_vulkan.cpp
	src/imgui_impl_sdl.cpp
	src/information_bar.cpp
	src/initial_setup.cpp
	src/license_install_dialog.cpp
	src/live_area.cpp
	src/main_menubar.cpp
	src/main_window.cpp
	src/manual.cpp
	src/mutexes_dialog.cpp
	src/pkg_install_dialog.cpp
	src/popups.cpp
	src/private.h
	src/semaphores_dialog.cpp
	src/sdl_window.cpp
	src/settings.cpp
	src/settings_dialog.cpp
	src/themes.cpp
	src/threads_dialog.cpp
	src/trophy_collection.cpp
	src/user_management.cpp
	src/online_storage.cpp
	src/v3kn_dialog.cpp
	src/overlay_dialog.cpp
	src/controls_dialog.cpp
	src/controllers_dialog.cpp
	src/allocations_dialog.cpp
	src/disassembly_dialog.cpp
	src/about_dialog.cpp
	src/vita3k_update.cpp
	src/welcome_dialog.cpp
)

# Add macOS-specific Objective-C++ files
if(APPLE)
	list(APPEND GUI_SOURCES
		include/gui-imgui/macos_helper.h
		src/macos_helper.mm
	)
endif()

add_library(gui-imgui STATIC ${GUI_SOURCES})

target_include_directories(gui-imgui PUBLIC include ${CMAKE_SOURCE_DIR}/vita3k)
target_link_libraries(gui-imgui PUBLIC app bgm_player compat config dialog emuenv host::dialog ime imgui lang regmgr np v3kn)
target_link_libraries(gui-imgui PRIVATE audio camera ctrl kernel miniz motion psvpfsparser pugixml::pugixml stb renderer packages SDL3::SDL3 touch vkutil concurrentqueue)
# Link macOS frameworks for Objective-C++ code
if(APPLE)
	target_link_libraries(gui-imgui PRIVATE
		"-framework Foundation"
		"-framework ImageIO"
		"-framework UniformTypeIdentifiers"
	)
elseif(ANDROID)
	target_link_libraries(gui-imgui PRIVATE android sdl-frontend)
endif()
if(TRACY_ENABLE_ON_CORE_COMPONENTS)
	target_link_libraries(gui-imgui PUBLIC tracy)
endif()
