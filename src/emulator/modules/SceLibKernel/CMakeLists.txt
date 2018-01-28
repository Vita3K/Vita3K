add_library(SceLibKernel STATIC include/SceLibKernel/exports.h src/SceLibKernel.cpp src/SceLibRng.cpp)
target_include_directories(SceLibKernel PUBLIC include)
target_link_libraries(SceLibKernel PRIVATE module)
