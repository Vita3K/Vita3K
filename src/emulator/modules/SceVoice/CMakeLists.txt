add_library(SceVoice STATIC include/SceVoice/exports.h src/SceVoice.cpp)
target_include_directories(SceVoice PUBLIC include)
target_link_libraries(SceVoice PRIVATE module)
