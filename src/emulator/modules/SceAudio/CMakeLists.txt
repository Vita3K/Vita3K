add_library(SceAudio STATIC include/SceAudio/exports.h src/SceAudio.cpp)
target_include_directories(SceAudio PUBLIC include)
target_link_libraries(SceAudio PRIVATE module)
