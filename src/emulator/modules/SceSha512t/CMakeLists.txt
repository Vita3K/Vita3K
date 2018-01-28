add_library(SceSha512t STATIC include/SceSha512t/exports.h src/SceSha512t.cpp)
target_include_directories(SceSha512t PUBLIC include)
target_link_libraries(SceSha512t PRIVATE module)
