add_library(SceSha1 STATIC include/SceSha1/exports.h src/SceSha1.cpp)
target_include_directories(SceSha1 PUBLIC include)
target_link_libraries(SceSha1 PRIVATE module)
