add_library(SceStdio STATIC include/SceStdio/exports.h src/SceStdio.cpp)
target_include_directories(SceStdio PUBLIC include)
target_link_libraries(SceStdio PRIVATE module)
