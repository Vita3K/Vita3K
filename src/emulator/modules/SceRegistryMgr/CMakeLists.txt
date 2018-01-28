add_library(SceRegistryMgr STATIC include/SceRegistryMgr/exports.h src/SceRegMgr.cpp src/SceRegMgrForGame.cpp src/SceRegMgrForSDK.cpp src/SceRegMgrService.cpp)
target_include_directories(SceRegistryMgr PUBLIC include)
target_link_libraries(SceRegistryMgr PRIVATE module)
