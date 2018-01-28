add_library(ScePerf STATIC include/ScePerf/exports.h src/ScePerf.cpp)
target_include_directories(ScePerf PUBLIC include)
target_link_libraries(ScePerf PRIVATE module)
