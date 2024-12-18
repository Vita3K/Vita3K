add_library(
	emuenv
	STATIC
	src/emuenv.cpp)

target_include_directories(emuenv INTERFACE include)
target_link_libraries(emuenv PUBLIC mem)
target_link_libraries(emuenv PRIVATE adhoc audio config ctrl dialog display ime io kernel motion net ngs nids np regmgr renderer touch gdbstub packages http)
