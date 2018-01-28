add_library(SceSfmt11213 STATIC include/SceSfmt11213/exports.h src/SceSfmt11213.cpp)
target_include_directories(SceSfmt11213 PUBLIC include)
target_link_libraries(SceSfmt11213 PRIVATE module)
