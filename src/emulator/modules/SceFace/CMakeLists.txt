add_library(SceFace STATIC include/SceFace/exports.h src/SceFace.cpp)
target_include_directories(SceFace PUBLIC include)
target_link_libraries(SceFace PRIVATE module)
