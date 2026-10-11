add_library(
	compat
	STATIC
	include/compat/functions.h
	include/compat/state.h
	src/compat.cpp
)

target_include_directories(compat PUBLIC include)
target_include_directories(compat PRIVATE ${CMAKE_SOURCE_DIR}/vita3k/util/include)
target_link_libraries(compat PUBLIC emuenv)
target_link_libraries(compat PRIVATE app config dialog pugixml::pugixml miniz util)
