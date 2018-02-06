add_library(
	util
	STATIC
	include/util/find.h
	include/util/lock_and_find.h
	include/util/resource.h
	include/util/string_convert.h
	src/util.cpp
)

target_include_directories(util PUBLIC include)
