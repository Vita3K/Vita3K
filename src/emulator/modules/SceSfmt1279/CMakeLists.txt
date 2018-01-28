add_library(SceSfmt1279 STATIC include/SceSfmt1279/exports.h src/SceSfmt1279.cpp)
target_include_directories(SceSfmt1279 PUBLIC include)
target_link_libraries(SceSfmt1279 PRIVATE module)
