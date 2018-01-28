add_library(SceAudioIn STATIC include/SceAudioIn/exports.h src/SceAudioIn.cpp)
target_include_directories(SceAudioIn PUBLIC include)
target_link_libraries(SceAudioIn PRIVATE module)
