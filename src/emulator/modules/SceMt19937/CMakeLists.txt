add_library(SceMt19937 STATIC include/SceMt19937/exports.h src/SceMt19937.cpp)
target_include_directories(SceMt19937 PUBLIC include)
target_link_libraries(SceMt19937 PRIVATE module)
