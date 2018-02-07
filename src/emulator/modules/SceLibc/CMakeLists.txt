add_library(SceLibc STATIC include/SceLibc/exports.h src/SceLibc.cpp src/SceLibm.cpp src/SceLibstdcxx.cpp)
target_include_directories(SceLibc PUBLIC include)
target_link_libraries(SceLibc PRIVATE module)
