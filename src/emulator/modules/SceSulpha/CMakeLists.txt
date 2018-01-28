add_library(SceSulpha STATIC include/SceSulpha/exports.h src/SceSulpha.cpp)
target_include_directories(SceSulpha PUBLIC include)
target_link_libraries(SceSulpha PRIVATE module)
