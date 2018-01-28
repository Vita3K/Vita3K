add_library(SceKernelDmacMgr STATIC include/SceKernelDmacMgr/exports.h src/SceDmacmgr.cpp)
target_include_directories(SceKernelDmacMgr PUBLIC include)
target_link_libraries(SceKernelDmacMgr PRIVATE module)
