add_library(
	ngs
	STATIC
	include/ngs/definitions/atrac9.h
	include/ngs/definitions/master.h
	include/ngs/definitions/player.h
	include/ngs/definitions/passthrough.h
	include/ngs/definitions/simple.h
	include/ngs/dsp/playback_rate.h
	include/ngs/modules/atrac9.h
	include/ngs/modules/equalizer.h
	include/ngs/modules/master.h
	include/ngs/modules/null.h
	include/ngs/modules/player.h
	include/ngs/modules/passthrough.h
	include/ngs/common.h
	include/ngs/scheduler.h
	include/ngs/state.h
	include/ngs/system.h

	src/definitions/atrac9.cpp
	src/definitions/master.cpp
	src/definitions/player.cpp
	src/definitions/passthrough.cpp
	src/definitions/simple.cpp
	src/dsp/playback_rate.cpp
	src/modules/atrac9.cpp
	src/modules/equalizer.cpp
	src/modules/master.cpp
	src/modules/null.cpp
	src/modules/player.cpp
	src/modules/passthrough.cpp
	src/ngs.cpp
	src/route.cpp
	src/scheduler.cpp)

target_include_directories(ngs PUBLIC include)
target_link_libraries(ngs PUBLIC codec)
target_link_libraries(ngs PRIVATE util mem kernel cpu SoundTouch)
