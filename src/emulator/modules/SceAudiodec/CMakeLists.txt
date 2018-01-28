add_library(SceAudiodec STATIC include/SceAudiodec/exports.h src/SceAudiodecUser.cpp)
target_include_directories(SceAudiodec PUBLIC include)
target_link_libraries(SceAudiodec PRIVATE module)
