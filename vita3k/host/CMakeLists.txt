add_library(
	host
	INTERFACE
	include/host/app_util.h
	include/host/state.h
	include/host/window.h
)

target_include_directories(host INTERFACE include)
target_link_libraries(host INTERFACE app audio config ctrl dialog display ime io kernel lang miniz net ngs nids np renderer touch gdbstub codec package)
