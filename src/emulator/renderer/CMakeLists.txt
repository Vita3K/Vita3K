add_library(
	renderer
	STATIC
	include/renderer/functions.h
	include/renderer/state.h
	include/renderer/texture_cache_state.h
	include/renderer/types.h
	src/attribute_formats.cpp
	src/compile_program.cpp
	src/draw.cpp
	src/functions.h
	src/load_shaders.cpp
	src/renderer.cpp
	src/profile.h
	src/sync_state.cpp
	src/texture.cpp
	src/texture_cache.cpp
	src/texture_formats.cpp
	src/texture_palette.cpp
	src/uniforms.cpp
)

target_include_directories(renderer PUBLIC include)
target_link_libraries(renderer PUBLIC crypto glutil vita-headers)
target_link_libraries(renderer PRIVATE shader gxm microprofile sdl2 util)
