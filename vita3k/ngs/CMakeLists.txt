add_library(
	ngs
	STATIC
	include/ngs/state.h
	src/allocator.cpp
	src/ngs.cpp
	src/scheduler.cpp
)

target_include_directories(ngs PUBLIC include)
target_link_libraries(ngs PRIVATE util mem)
