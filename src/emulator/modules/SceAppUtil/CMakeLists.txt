add_library(SceAppUtil STATIC include/SceAppUtil/exports.h src/SceAppUtil.cpp src/SceAppUtilCache.cpp)
target_include_directories(SceAppUtil PUBLIC include)
target_link_libraries(SceAppUtil PRIVATE module)
