add_library(SceMotion STATIC include/SceMotion/exports.h src/SceMotion.cpp)
target_include_directories(SceMotion PUBLIC include)
target_link_libraries(SceMotion PRIVATE module)
