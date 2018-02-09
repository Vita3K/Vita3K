add_library(SceFpu STATIC include/SceFpu/exports.h src/SceFpu.cpp)
target_include_directories(SceFpu PUBLIC include)
target_link_libraries(SceFpu PRIVATE module)
