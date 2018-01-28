add_library(SceUdcd STATIC include/SceUdcd/exports.h src/SceUdcd.cpp)
target_include_directories(SceUdcd PUBLIC include)
target_link_libraries(SceUdcd PRIVATE module)
