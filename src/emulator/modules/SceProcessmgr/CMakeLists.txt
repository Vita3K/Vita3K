add_library(SceProcessmgr STATIC include/SceProcessmgr/exports.h src/SceProcessmgr.cpp)
target_include_directories(SceProcessmgr PUBLIC include)
target_link_libraries(SceProcessmgr PRIVATE module)
