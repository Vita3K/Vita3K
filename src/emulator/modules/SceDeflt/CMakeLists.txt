add_library(SceDeflt STATIC include/SceDeflt/exports.h src/SceDeflt.cpp)
target_include_directories(SceDeflt PUBLIC include)
target_link_libraries(SceDeflt PRIVATE module)
