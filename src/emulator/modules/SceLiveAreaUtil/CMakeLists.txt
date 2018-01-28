add_library(SceLiveAreaUtil STATIC include/SceLiveAreaUtil/exports.h src/SceLiveAreaUtil.cpp)
target_include_directories(SceLiveAreaUtil PUBLIC include)
target_link_libraries(SceLiveAreaUtil PRIVATE module)
