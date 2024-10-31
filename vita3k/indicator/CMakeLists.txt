add_library(
	indicator
	STATIC
	include/indicator/state.h
	src/indicator.cpp
)

target_include_directories(indicator PUBLIC include)
target_link_libraries(indicator PUBLIC emuenv io util)
target_link_libraries(indicator PRIVATE pugixml::pugixml)
