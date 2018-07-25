add_library(
	renderer
	STATIC
	include/renderer/functions.h
	include/renderer/state.h
	include/renderer/texture_cache_state.h
	include/renderer/types.h
	src/draw.cpp
	src/functions.h
	src/renderer.cpp
	src/profile.h
	src/texture_cache.cpp
	src/texture_formats.cpp
)

target_include_directories(renderer PUBLIC include)
target_link_libraries(renderer PUBLIC crypto glutil vita-headers)
target_link_libraries(renderer PRIVATE gxm microprofile sdl2 util)
