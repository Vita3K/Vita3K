add_library(SceLibDbg STATIC include/SceLibDbg/exports.h src/SceDbg.cpp)
target_include_directories(SceLibDbg PUBLIC include)
target_link_libraries(SceLibDbg PRIVATE module)
