add_library(
	gxm
	STATIC
	include/gxm/functions.h
	include/gxm/state.h
	include/gxm/types.h
	src/gxm.cpp
	src/texture_formats.cpp
)

target_include_directories(gxm PUBLIC include)
target_link_libraries(gxm PUBLIC crypto glutil mem sdl2 rpcs3 threads vita-headers)
target_link_libraries(gxm PRIVATE microprofile)
