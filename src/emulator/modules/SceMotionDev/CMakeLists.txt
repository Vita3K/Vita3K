add_library(SceMotionDev STATIC include/SceMotionDev/exports.h src/SceMotionDev.cpp)
target_include_directories(SceMotionDev PUBLIC include)
target_link_libraries(SceMotionDev PRIVATE module)
