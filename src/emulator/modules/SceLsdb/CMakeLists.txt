add_library(SceLsdb STATIC include/SceLsdb/exports.h src/SceLsdb.cpp)
target_include_directories(SceLsdb PUBLIC include)
target_link_libraries(SceLsdb PRIVATE module)
