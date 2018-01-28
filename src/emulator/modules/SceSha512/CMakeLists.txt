add_library(SceSha512 STATIC include/SceSha512/exports.h src/SceSha512.cpp)
target_include_directories(SceSha512 PUBLIC include)
target_link_libraries(SceSha512 PRIVATE module)
