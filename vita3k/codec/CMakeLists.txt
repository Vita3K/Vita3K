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
    find_package(Python3 QUIET COMPONENTS Interpreter)
    find_program(FFMPEG_TEST_EXECUTABLE ffmpeg)
    if(Python3_Interpreter_FOUND AND FFMPEG_TEST_EXECUTABLE)
        add_executable(codec-player-tests tests/player_tests.cpp)
        target_link_libraries(codec-player-tests PRIVATE codec ffmpeg util)
        find_package(Threads REQUIRED)
        set(AVPLAYER_HLE_TEST_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/avplayer_hle_test.cpp)
        add_custom_command(OUTPUT ${AVPLAYER_HLE_TEST_SOURCE}
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_avplayer_hle_test.py ${AVPLAYER_HLE_TEST_SOURCE}
            DEPENDS tests/generate_avplayer_hle_test.py ../modules/SceAvPlayer/SceAvPlayer.cpp
            VERBATIM)
        add_executable(avplayer-hle-tests ${AVPLAYER_HLE_TEST_SOURCE})
        target_link_libraries(avplayer-hle-tests PRIVATE codec ffmpeg util Threads::Threads)
        if(MSVC)
            target_compile_options(avplayer-hle-tests PRIVATE /UNDEBUG)
        else()
            target_compile_options(avplayer-hle-tests PRIVATE -UNDEBUG)
        endif()
        add_test(NAME codec-player
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_player_tests.py
                $<TARGET_FILE:codec-player-tests> ${FFMPEG_TEST_EXECUTABLE} $<TARGET_FILE:avplayer-hle-tests>)
    endif()
endif()
