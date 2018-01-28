add_library(SceUlt STATIC include/SceUlt/exports.h src/SceUlt.cpp)
target_include_directories(SceUlt PUBLIC include)
target_link_libraries(SceUlt PRIVATE module)
