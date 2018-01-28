add_library(ScePaf STATIC include/ScePaf/exports.h src/ScePafStdc.cpp)
target_include_directories(ScePaf PUBLIC include)
target_link_libraries(ScePaf PRIVATE module)
