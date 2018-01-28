add_library(SceSfmt86243 STATIC include/SceSfmt86243/exports.h src/SceSfmt86243.cpp)
target_include_directories(SceSfmt86243 PUBLIC include)
target_link_libraries(SceSfmt86243 PRIVATE module)
