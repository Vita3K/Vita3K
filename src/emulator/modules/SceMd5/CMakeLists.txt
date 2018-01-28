add_library(SceMd5 STATIC include/SceMd5/exports.h src/SceMd5.cpp)
target_include_directories(SceMd5 PUBLIC include)
target_link_libraries(SceMd5 PRIVATE module)
