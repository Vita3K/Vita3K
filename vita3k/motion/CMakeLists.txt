add_library(
	motion
	STATIC
	src/motion.cpp
	src/motion_input.cpp
)

target_include_directories(motion PUBLIC include)
target_link_libraries(motion PUBLIC emuenv SDL3::Headers util)
target_link_libraries(motion PRIVATE ctrl)
