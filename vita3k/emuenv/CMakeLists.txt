add_library(
	emuenv
	STATIC
	src/emuenv.cpp)

target_include_directories(emuenv INTERFACE include)
target_link_libraries(emuenv PUBLIC mem)
target_link_libraries(emuenv PRIVATE app audio config ctrl dialog display ime io kernel lang miniz net ngs nids np renderer touch gdbstub codec packages)
