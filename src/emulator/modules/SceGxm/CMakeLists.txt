add_library(SceGxm STATIC include/SceGxm/exports.h src/gxm.h src/SceGxm.cpp)
target_include_directories(SceGxm PUBLIC include)
target_link_libraries(SceGxm PRIVATE glutil module)
