add_library(
	bridge
	STATIC
	include/bridge/imgui_impl_sdl_gl3.h
	src/imgui_impl_sdl_gl3.cpp
)

target_include_directories(bridge PUBLIC include)
target_link_libraries(bridge PUBLIC glutil imgui sdl2)
