add_library(
	lang
	STATIC
	include/lang/state.h
	include/lang/functions.h
	src/lang.cpp
)

target_include_directories(lang PUBLIC include)
target_link_libraries(lang PUBLIC host)
target_link_libraries(lang PRIVATE pugixml::pugixml)
