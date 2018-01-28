add_library(SceShellSvc STATIC include/SceShellSvc/exports.h src/SceShellUtil.cpp)
target_include_directories(SceShellSvc PUBLIC include)
target_link_libraries(SceShellSvc PRIVATE module)
