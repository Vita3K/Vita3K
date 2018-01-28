add_library(SceSfmt132049 STATIC include/SceSfmt132049/exports.h src/SceSfmt132049.cpp)
target_include_directories(SceSfmt132049 PUBLIC include)
target_link_libraries(SceSfmt132049 PRIVATE module)
