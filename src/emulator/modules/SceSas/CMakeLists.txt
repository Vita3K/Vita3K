add_library(SceSas STATIC include/SceSas/exports.h src/SceSas.cpp)
target_include_directories(SceSas PUBLIC include)
target_link_libraries(SceSas PRIVATE module)
