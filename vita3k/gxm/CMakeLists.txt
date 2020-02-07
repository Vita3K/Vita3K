add_library(
	gxm
	STATIC
	include/gxm/functions.h
	include/gxm/state.h
	include/gxm/types.h
	src/attributes.cpp
	src/gxp.cpp
	src/textures.cpp
)

target_include_directories(gxm PUBLIC include)
target_link_libraries(gxm PUBLIC mem rpcs3 threads)
target_link_libraries(gxm PRIVATE)
