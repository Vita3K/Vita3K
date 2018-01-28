add_library(SceKernelThreadMgr STATIC include/SceKernelThreadMgr/exports.h src/SceDebugLed.cpp src/SceDipsw.cpp src/SceThreadmgr.cpp src/SceThreadmgrCoredumpTime.cpp)
target_include_directories(SceKernelThreadMgr PUBLIC include)
target_link_libraries(SceKernelThreadMgr PRIVATE module)
