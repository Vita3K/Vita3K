add_library(SceFios2 STATIC include/SceFios2/exports.h src/SceFios2.cpp)
target_include_directories(SceFios2 PUBLIC include)
target_link_libraries(SceFios2 PRIVATE module)
