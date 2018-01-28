add_library(SceNgs STATIC include/SceNgs/exports.h src/SceNgs.cpp)
target_include_directories(SceNgs PUBLIC include)
target_link_libraries(SceNgs PRIVATE module)
