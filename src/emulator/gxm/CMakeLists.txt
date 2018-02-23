add_library(
	gxm
	INTERFACE
)

target_include_directories(gxm INTERFACE include)
target_link_libraries(gxm INTERFACE glutil mem sdl2 vita-headers)
