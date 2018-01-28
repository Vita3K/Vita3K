add_library(SceSha384 STATIC include/SceSha384/exports.h src/SceSha384.cpp)
target_include_directories(SceSha384 PUBLIC include)
target_link_libraries(SceSha384 PRIVATE module)
