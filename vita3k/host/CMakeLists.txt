add_library(
	host
	STATIC
	include/host/app_util.h
	include/host/functions.h
	include/host/import_var.h
	include/host/import_fn.h
	include/host/load_self.h
	include/host/pkg.h
	include/host/sce_types.h
	include/host/sfo.h
	include/host/state.h
	include/host/window.h
	src/load_self.cpp
	src/pkg.cpp
	src/pup.cpp
	src/sce_utils.cpp
	src/sfo.cpp
)

target_include_directories(host PUBLIC include ${PSVPFSPARSER_INCLUDE_DIR})
target_link_libraries(host PUBLIC psvpfsparser app audio config ctrl dialog io kernel miniz net ngs nids np renderer sdl2 touch)
if(USE_GDBSTUB)
	target_link_libraries(host PUBLIC gdbstub)
endif()
target_link_libraries(host PRIVATE elfio::elfio FAT16 vita-toolchain)
