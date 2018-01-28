add_library(SceSha256 STATIC include/SceSha256/exports.h src/SceSha256.cpp)
target_include_directories(SceSha256 PUBLIC include)
target_link_libraries(SceSha256 PRIVATE module)
