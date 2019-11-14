add_library(
	ngs
	STATIC
	include/ngs/modules/atrac9.h
	include/ngs/modules/master.h
	include/ngs/modules/player.h
	include/ngs/common.h
	include/ngs/mempool.h
	include/ngs/scheduler.h
	include/ngs/state.h
	include/ngs/system.h
	src/modules/atrac9.cpp
	src/modules/master.cpp
	src/modules/player.cpp
	src/allocator.cpp
	src/ngs.cpp
	src/scheduler.cpp
)

target_include_directories(ngs PUBLIC include)
target_link_libraries(ngs PRIVATE util mem ffmpeg)
