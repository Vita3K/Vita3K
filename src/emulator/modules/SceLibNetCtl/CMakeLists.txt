add_library(SceLibNetCtl STATIC include/SceLibNetCtl/exports.h src/SceNetCtl.cpp)
target_include_directories(SceLibNetCtl PUBLIC include)
target_link_libraries(SceLibNetCtl PRIVATE module)
