add_library(SceIofilemgr STATIC include/SceIofilemgr/exports.h src/SceIofilemgr.cpp)
target_include_directories(SceIofilemgr PUBLIC include)
target_link_libraries(SceIofilemgr PRIVATE module)
