add_library(SceShutterSound STATIC include/SceShutterSound/exports.h src/SceShutterSound.cpp)
target_include_directories(SceShutterSound PUBLIC include)
target_link_libraries(SceShutterSound PRIVATE module)
