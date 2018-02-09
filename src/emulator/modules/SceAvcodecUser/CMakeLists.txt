add_library(SceAvcodecUser STATIC include/SceAvcodecUser/exports.h src/SceVideoencUser.cpp)
target_include_directories(SceAvcodecUser PUBLIC include)
target_link_libraries(SceAvcodecUser PRIVATE module)
