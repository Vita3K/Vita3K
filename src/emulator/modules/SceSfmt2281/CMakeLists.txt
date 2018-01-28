add_library(SceSfmt2281 STATIC include/SceSfmt2281/exports.h src/SceSfmt2281.cpp)
target_include_directories(SceSfmt2281 PUBLIC include)
target_link_libraries(SceSfmt2281 PRIVATE module)
