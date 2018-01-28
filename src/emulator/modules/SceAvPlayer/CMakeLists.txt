add_library(SceAvPlayer STATIC include/SceAvPlayer/exports.h src/SceAvPlayer.cpp)
target_include_directories(SceAvPlayer PUBLIC include)
target_link_libraries(SceAvPlayer PRIVATE module)
