add_library(SceAvcodec STATIC include/SceAvcodec/exports.h src/SceAvcodec.cpp)
target_include_directories(SceAvcodec PUBLIC include)
target_link_libraries(SceAvcodec PRIVATE module)
