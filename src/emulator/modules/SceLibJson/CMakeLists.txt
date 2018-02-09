add_library(SceLibJson STATIC include/SceLibJson/exports.h src/SceLibJson.cpp)
target_include_directories(SceLibJson PUBLIC include)
target_link_libraries(SceLibJson PRIVATE module)
