add_library(
	sdl-frontend
	STATIC
	include/sdl-frontend/session.h
	src/frame_host.cpp
	src/frame_host.h
	src/session.cpp
)

target_include_directories(sdl-frontend PUBLIC include)
target_link_libraries(sdl-frontend PUBLIC emuenv)
target_link_libraries(sdl-frontend PRIVATE app audio config ctrl dialog ime motion renderer touch util SDL3::SDL3)
