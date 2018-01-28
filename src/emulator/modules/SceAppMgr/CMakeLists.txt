add_library(SceAppMgr STATIC include/SceAppMgr/exports.h src/SceAppMgr.cpp src/SceAppMgrUser.cpp src/SceSharedFb.cpp)
target_include_directories(SceAppMgr PUBLIC include)
target_link_libraries(SceAppMgr PRIVATE module)
