add_library(
	ngs
	STATIC
	src/modules/atrac9.cpp
	src/modules/equalizer.cpp
	src/modules/master.cpp
	src/modules/null.cpp
	src/modules/player.cpp
	src/modules/passthrough.cpp
	src/definitions.cpp
	src/ngs.cpp
	src/route.cpp
	src/scheduler.cpp)

target_include_directories(ngs PUBLIC include)
target_link_libraries(ngs PUBLIC codec)
target_link_libraries(ngs PRIVATE util mem kernel cpu ffmpeg)
