add_library(SceBt STATIC include/SceBt/exports.h src/SceBt.cpp)
target_include_directories(SceBt PUBLIC include)
target_link_libraries(SceBt PRIVATE module)
