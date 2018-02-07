add_library(SceHttp STATIC include/SceHttp/exports.h src/SceHttp.cpp)
target_include_directories(SceHttp PUBLIC include)
target_link_libraries(SceHttp PRIVATE module)
