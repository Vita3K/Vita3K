add_library(SceNpManager STATIC include/SceNpManager/exports.h src/SceNpManager.cpp)
target_include_directories(SceNpManager PUBLIC include)
target_link_libraries(SceNpManager PRIVATE module)
