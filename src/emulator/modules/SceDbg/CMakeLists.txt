add_library(SceDbg STATIC include/SceDbg/exports.h src/SceDbg.cpp)
target_include_directories(SceDbg PUBLIC include)
target_link_libraries(SceDbg PRIVATE module)
