add_library(SceSqlite STATIC include/SceSqlite/exports.h src/SceSqlite.cpp)
target_include_directories(SceSqlite PUBLIC include)
target_link_libraries(SceSqlite PRIVATE module)
