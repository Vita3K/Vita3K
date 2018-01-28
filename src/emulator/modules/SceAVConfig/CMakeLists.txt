add_library(SceAVConfig STATIC include/SceAVConfig/exports.h src/SceAVConfig.cpp)
target_include_directories(SceAVConfig PUBLIC include)
target_link_libraries(SceAVConfig PRIVATE module)
