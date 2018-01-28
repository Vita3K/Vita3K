add_library(SceNearUtil STATIC include/SceNearUtil/exports.h src/SceNearUtil.cpp)
target_include_directories(SceNearUtil PUBLIC include)
target_link_libraries(SceNearUtil PRIVATE module)
