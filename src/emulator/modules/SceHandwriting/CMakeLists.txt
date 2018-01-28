add_library(SceHandwriting STATIC include/SceHandwriting/exports.h src/SceHandwriting.cpp)
target_include_directories(SceHandwriting PUBLIC include)
target_link_libraries(SceHandwriting PRIVATE module)
