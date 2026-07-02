add_library(
    codec
    STATIC
    include/codec/state.h
    include/codec/types.h
    src/atrac9.cpp
    src/decoder.cpp
    src/aac.cpp
    src/h264.cpp
    src/mjpeg.cpp
    src/mp3.cpp
    src/pcm.cpp
    src/player.cpp
)

target_include_directories(codec PUBLIC include)
target_link_libraries(codec PRIVATE ffmpeg libatrac9 util) 

if(NOT ANDROID)
	add_executable(
		codec-tests
		tests/atrac9_tests.cpp
	)

	target_include_directories(codec-tests PRIVATE include)
	target_link_libraries(codec-tests PRIVATE codec googletest util)
	add_test(NAME codec COMMAND codec-tests)
endif()
