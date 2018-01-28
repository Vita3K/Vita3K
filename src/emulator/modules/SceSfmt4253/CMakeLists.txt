add_library(SceSfmt4253 STATIC include/SceSfmt4253/exports.h src/SceSfmt4253.cpp)
target_include_directories(SceSfmt4253 PUBLIC include)
target_link_libraries(SceSfmt4253 PRIVATE module)
