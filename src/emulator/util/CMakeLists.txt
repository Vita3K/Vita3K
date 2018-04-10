add_library(
	util
	STATIC
	include/util/v3k_assert.h
	include/util/find.h
	include/util/lock_and_find.h
	include/util/resource.h
	include/util/string_convert.h
	include/util/log.h
	src/util.cpp
)

target_include_directories(util PUBLIC include)
target_link_libraries(util PUBLIC spdlog)
