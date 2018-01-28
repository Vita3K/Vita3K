add_library(SceSasUser STATIC include/SceSasUser/exports.h src/SceSas.cpp)
target_include_directories(SceSasUser PUBLIC include)
target_link_libraries(SceSasUser PRIVATE module)
