add_library(SceGpuEs4 STATIC include/SceGpuEs4/exports.h src/SceGpuEs4ForUser.cpp)
target_include_directories(SceGpuEs4 PUBLIC include)
target_link_libraries(SceGpuEs4 PRIVATE module)
