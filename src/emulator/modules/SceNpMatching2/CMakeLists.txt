add_library(SceNpMatching2 STATIC include/SceNpMatching2/exports.h src/SceNpMatching2.cpp)
target_include_directories(SceNpMatching2 PUBLIC include)
target_link_libraries(SceNpMatching2 PRIVATE module)
