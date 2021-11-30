add_library(
	motion
	STATIC
	include/motion/functions.h
	include/motion/motion.h
	include/motion/state.h
	src/motion.cpp
)

target_include_directories(motion PUBLIC include)
target_link_libraries(motion PUBLIC emuenv sdl2 util)
