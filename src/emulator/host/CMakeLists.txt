add_library(
	host
	STATIC
	include/host/app_util.h
	include/host/functions.h
	include/host/get_sfo.h
	include/host/import_fn.h
	include/host/load_self.h
	include/host/sfo.h
	include/host/state.h
	src/get_sfo.cpp
	src/host.cpp
	src/load_self.cpp
	src/sfo.cpp
)

target_include_directories(host PUBLIC include)
target_link_libraries(host PUBLIC audio bridge config ctrl dialog kernel io net nids np renderer touch)
if(USE_GDBSTUB)
	target_link_libraries(host PUBLIC gdbstub)
endif()
target_link_libraries(host PRIVATE elfio vita-toolchain)
