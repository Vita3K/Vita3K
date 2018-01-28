add_library(SceLibc STATIC include/SceLibc/exports.h src/SceLibc.cpp)
target_include_directories(SceLibc PUBLIC include)
target_link_libraries(SceLibc PRIVATE module)
