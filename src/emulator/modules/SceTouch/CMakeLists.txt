add_library(SceTouch STATIC include/SceTouch/exports.h src/SceTouch.cpp)
target_include_directories(SceTouch PUBLIC include)
target_link_libraries(SceTouch PRIVATE module)
