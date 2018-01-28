add_library(SceLibHttp STATIC include/SceLibHttp/exports.h src/SceHttp.cpp)
target_include_directories(SceLibHttp PUBLIC include)
target_link_libraries(SceLibHttp PRIVATE module)
