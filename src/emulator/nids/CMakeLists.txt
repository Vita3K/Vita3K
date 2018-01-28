add_library(
	nids
	STATIC
	include/nids/functions.h
	include/nids/nids.h
	src/nids.cpp
)

target_include_directories(nids PUBLIC include)
