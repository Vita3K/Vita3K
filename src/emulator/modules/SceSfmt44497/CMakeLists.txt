add_library(SceSfmt44497 STATIC include/SceSfmt44497/exports.h src/SceSfmt44497.cpp)
target_include_directories(SceSfmt44497 PUBLIC include)
target_link_libraries(SceSfmt44497 PRIVATE module)
