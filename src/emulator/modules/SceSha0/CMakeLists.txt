add_library(SceSha0 STATIC include/SceSha0/exports.h src/SceSha0.cpp)
target_include_directories(SceSha0 PUBLIC include)
target_link_libraries(SceSha0 PRIVATE module)
