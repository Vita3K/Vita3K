add_library(SceScreenShot STATIC include/SceScreenShot/exports.h src/SceScreenShot.cpp)
target_include_directories(SceScreenShot PUBLIC include)
target_link_libraries(SceScreenShot PRIVATE module)
