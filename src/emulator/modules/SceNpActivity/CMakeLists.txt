add_library(SceNpActivity STATIC include/SceNpActivity/exports.h src/SceNpActivity.cpp)
target_include_directories(SceNpActivity PUBLIC include)
target_link_libraries(SceNpActivity PRIVATE module)
