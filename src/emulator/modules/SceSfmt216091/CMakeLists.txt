add_library(SceSfmt216091 STATIC include/SceSfmt216091/exports.h src/SceSfmt216091.cpp)
target_include_directories(SceSfmt216091 PUBLIC include)
target_link_libraries(SceSfmt216091 PRIVATE module)
