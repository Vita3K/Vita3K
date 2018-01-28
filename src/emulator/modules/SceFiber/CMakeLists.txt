add_library(SceFiber STATIC include/SceFiber/exports.h src/SceFiber.cpp)
target_include_directories(SceFiber PUBLIC include)
target_link_libraries(SceFiber PRIVATE module)
