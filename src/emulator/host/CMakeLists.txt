add_library(
	host
	STATIC
	include/host/app_util.h
	include/host/functions.h
	include/host/import_fn.h
	include/host/load_self.h
	include/host/sfo.h
	include/host/state.h
	include/host/window.h
	src/load_self.cpp
	src/sfo.cpp
)

target_include_directories(host PUBLIC include)
target_link_libraries(host PUBLIC audio config ctrl dialog io kernel net nids np renderer sdl2 touch)
if(USE_GDBSTUB)
	target_link_libraries(host PUBLIC gdbstub)
endif()
target_link_libraries(host PRIVATE elfio vita-toolchain)
