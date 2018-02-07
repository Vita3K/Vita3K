add_library(SceCompat STATIC include/SceCompat/exports.h src/SceCompat.cpp)
target_include_directories(SceCompat PUBLIC include)
target_link_libraries(SceCompat PRIVATE module)
