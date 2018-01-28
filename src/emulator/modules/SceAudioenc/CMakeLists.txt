add_library(SceAudioenc STATIC include/SceAudioenc/exports.h src/SceAudioencUser.cpp)
target_include_directories(SceAudioenc PUBLIC include)
target_link_libraries(SceAudioenc PRIVATE module)
