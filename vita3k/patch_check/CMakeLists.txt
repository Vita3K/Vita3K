add_library(
	patch_check
	STATIC
	include/patch_check/functions.h
	include/patch_check/state.h
	src/patch_check.cpp
)

target_include_directories(patch_check PUBLIC include)
target_link_libraries(patch_check PUBLIC emuenv util packages)
target_link_libraries(patch_check PRIVATE pugixml::pugixml)
