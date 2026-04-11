add_library(
	regmgr
	STATIC
	include/regmgr/functions.h
	include/regmgr/state.h
	include/regmgr/types.h
	src/regmgr.cpp
)

target_include_directories(regmgr PUBLIC include)
target_include_directories(regmgr PRIVATE)
target_link_libraries(regmgr PUBLIC util)
