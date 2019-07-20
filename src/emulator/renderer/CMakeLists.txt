add_library(
	renderer
	STATIC
	include/renderer/commands.h
	include/renderer/functions.h
	include/renderer/state.h
	include/renderer/types.h
	src/gl/attribute_formats.cpp
	src/gl/compile_program.cpp
	src/gl/draw.cpp
	src/gl/functions.h
	src/gl/profile.h
	src/gl/renderer.cpp
	src/gl/sync_state.cpp
	src/gl/texture.cpp
	src/gl/texture_cache.cpp
	src/gl/texture_formats.cpp
	src/gl/texture_palette.cpp
	src/gl/uniforms.cpp
	src/gl/state.h
	src/gl/texture_cache_state.h
	src/gl/load_shaders.cpp
	src/gl/types.h
	src/creation.cpp
	src/batch.cpp
	src/driver_functions.h
	src/scene.cpp
	src/state_set.cpp
	src/sync.cpp
	src/uniforms.cpp
)

target_include_directories(renderer PUBLIC include)
target_link_libraries(renderer PUBLIC crypto shader glutil threads config vita-headers util)
target_link_libraries(renderer PRIVATE sdl2)
