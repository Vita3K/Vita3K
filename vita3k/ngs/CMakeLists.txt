add_library(
	ngs
	STATIC
	include/ngs/ngs.h
	src/allocator.cpp
	src/ngs.cpp
)

target_include_directories(ngs PUBLIC include)
target_link_libraries(ngs PRIVATE util mem)
