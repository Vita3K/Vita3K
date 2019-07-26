add_library(
	renderer
	STATIC
	include/renderer/commands.h
	include/renderer/functions.h
	include/renderer/profile.h
	include/renderer/state.h
	include/renderer/texture_cache_state.h
	include/renderer/types.h
	src/gl/attribute_formats.cpp
	src/gl/compile_program.cpp
	src/gl/draw.cpp
	src/gl/functions.h
	src/gl/load_shaders.cpp
	src/gl/renderer.cpp
	src/gl/state.h
	src/gl/sync_state.cpp
	src/gl/texture_formats.cpp
	src/gl/texture.cpp
	src/gl/types.h
	src/gl/uniforms.cpp
	src/batch.cpp
	src/creation.cpp
	src/driver_functions.h
	src/renderer.cpp
	src/scene.cpp
	src/state_set.cpp
	src/sync.cpp
	src/texture_cache.cpp
	src/texture_format.cpp
	src/texture_palette.cpp
	src/uniforms.cpp
)

target_include_directories(renderer PUBLIC include)
target_link_libraries(renderer PUBLIC crypto shader glutil threads config vita-headers util)
