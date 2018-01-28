add_library(SceNpScore STATIC include/SceNpScore/exports.h src/SceNpScore.cpp)
target_include_directories(SceNpScore PUBLIC include)
target_link_libraries(SceNpScore PRIVATE module)
