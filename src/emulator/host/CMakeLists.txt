add_library(
	host
	STATIC
	include/host/app_util.h
	include/host/functions.h
	include/host/import_fn.h
	include/host/sfo.h
	include/host/state.h
	src/host.cpp
	src/sfo.cpp
)

target_include_directories(host PUBLIC include)
target_link_libraries(host PUBLIC audio config ctrl gui gdbstub kernel io net nids np renderer touch)
