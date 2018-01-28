add_library(SceJpeg STATIC include/SceJpeg/exports.h src/SceJpegUser.cpp)
target_include_directories(SceJpeg PUBLIC include)
target_link_libraries(SceJpeg PRIVATE module)
