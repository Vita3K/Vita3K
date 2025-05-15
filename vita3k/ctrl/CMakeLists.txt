add_library(
	ctrl
	STATIC
	include/ctrl/ctrl.h
	include/ctrl/functions.h
	include/ctrl/state.h
	src/ctrl.cpp
)

target_include_directories(ctrl PUBLIC include)
target_link_libraries(ctrl PUBLIC emuenv SDL3::SDL3 util)
target_link_libraries(ctrl PRIVATE config dialog display kernel)

