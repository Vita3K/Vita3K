add_library(SceLiveArea STATIC include/SceLiveArea/exports.h src/SceLiveAreaUtil.cpp)
target_include_directories(SceLiveArea PUBLIC include)
target_link_libraries(SceLiveArea PRIVATE module)
