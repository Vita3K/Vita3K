add_library(SceNetCtl STATIC include/SceNetCtl/exports.h src/SceNetCtl.cpp)
target_include_directories(SceNetCtl PUBLIC include)
target_link_libraries(SceNetCtl PRIVATE module)
