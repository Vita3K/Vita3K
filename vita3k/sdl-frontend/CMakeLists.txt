add_library(
	sdl-frontend
	STATIC
	include/sdl-frontend/app_selector.h
	include/sdl-frontend/main_window.h
	src/app_selector.cpp
	src/main_window.cpp
	src/session.cpp
	src/users.cpp
	include/sdl-frontend/font_renderer.h
	include/sdl-frontend/functions.h
	include/sdl-frontend/state.h
	src/initial_setup.cpp
	src/ui_utils.cpp
)

target_include_directories(sdl-frontend PUBLIC include)
target_link_libraries(sdl-frontend PUBLIC emuenv)
target_link_libraries(sdl-frontend PRIVATE app audio bgm_player config ctrl dialog ime motion overlay renderer touch util vita_shell SDL3::SDL3 sdl)
