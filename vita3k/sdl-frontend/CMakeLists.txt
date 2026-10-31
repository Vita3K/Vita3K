add_library(
	sdl-frontend
	STATIC
	include/sdl-frontend/font_renderer.h
	include/sdl-frontend/functions.h
	include/sdl-frontend/main_window.h
	include/sdl-frontend/state.h
	src/app_selector.cpp
	src/initial_setup.cpp
	src/main_window.cpp
	src/session.cpp
	src/ui_utils.cpp
)

target_include_directories(sdl-frontend PUBLIC include)
target_link_libraries(sdl-frontend PUBLIC emuenv)
target_link_libraries(sdl-frontend PRIVATE app audio config ctrl dialog ime motion overlay renderer touch util SDL3::SDL3 sdl)
