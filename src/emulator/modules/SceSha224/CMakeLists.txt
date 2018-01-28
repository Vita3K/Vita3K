add_library(SceSha224 STATIC include/SceSha224/exports.h src/SceSha224.cpp)
target_include_directories(SceSha224 PUBLIC include)
target_link_libraries(SceSha224 PRIVATE module)
