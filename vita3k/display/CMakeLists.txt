add_library(
	display
	STATIC
	include/display/state.h
	include/display/functions.h
	src/display.cpp
)

target_include_directories(display PUBLIC include)
target_link_libraries(display PUBLIC emuenv kernel)
target_link_libraries(display PRIVATE kernel touch renderer dialog motion)
