add_library(
	ctrl
	STATIC
	include/ctrl/ctrl.h
	include/ctrl/functions.h
	include/ctrl/state.h
	include/ctrl/udp_client.h
	include/ctrl/udp_protocol.h
	src/ctrl.cpp
	src/udp_client.cpp
	src/udp_protocol.cpp
)

target_include_directories(ctrl PUBLIC include)
target_link_libraries(ctrl PUBLIC emuenv sdl2 util)
target_link_libraries(ctrl PRIVATE config dialog display kernel)

