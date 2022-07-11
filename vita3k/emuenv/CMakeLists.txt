add_library(
	emuenv
	INTERFACE
	include/emuenv/app_util.h
	include/emuenv/state.h
	include/emuenv/window.h
)

target_include_directories(emuenv INTERFACE include)
target_link_libraries(emuenv INTERFACE app audio config ctrl dialog display ime io kernel lang miniz net ngs nids np renderer touch gdbstub codec packages)
