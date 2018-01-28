add_library(SceSfmt607 STATIC include/SceSfmt607/exports.h src/SceSfmt607.cpp)
target_include_directories(SceSfmt607 PUBLIC include)
target_link_libraries(SceSfmt607 PRIVATE module)
