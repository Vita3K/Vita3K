add_library(SceSfmt19937 STATIC include/SceSfmt19937/exports.h src/SceSfmt19937.cpp)
target_include_directories(SceSfmt19937 PUBLIC include)
target_link_libraries(SceSfmt19937 PRIVATE module)
