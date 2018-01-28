add_library(SceJpegEnc STATIC include/SceJpegEnc/exports.h src/SceJpegEncUser.cpp)
target_include_directories(SceJpegEnc PUBLIC include)
target_link_libraries(SceJpegEnc PRIVATE module)
