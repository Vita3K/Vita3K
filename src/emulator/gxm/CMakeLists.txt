add_library(
	gxm
	STATIC
	include/gxm/functions.h
	include/gxm/state.h
	include/gxm/types.h
	src/gxm.cpp
)

target_include_directories(gxm PUBLIC include)
target_link_libraries(gxm PUBLIC glutil mem sdl2 vita-headers)
target_link_libraries(gxm PRIVATE crypto microprofile reporting)
