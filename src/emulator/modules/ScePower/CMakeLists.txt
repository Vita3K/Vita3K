add_library(ScePower STATIC include/ScePower/exports.h src/ScePower.cpp)
target_include_directories(ScePower PUBLIC include)
target_link_libraries(ScePower PRIVATE module)
