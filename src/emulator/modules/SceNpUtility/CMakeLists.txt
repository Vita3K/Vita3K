add_library(SceNpUtility STATIC include/SceNpUtility/exports.h src/SceNpUtility.cpp)
target_include_directories(SceNpUtility PUBLIC include)
target_link_libraries(SceNpUtility PRIVATE module)
