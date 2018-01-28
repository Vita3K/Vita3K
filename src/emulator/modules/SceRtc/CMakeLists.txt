add_library(SceRtc STATIC include/SceRtc/exports.h src/SceRtc.cpp src/SceRtcUser.cpp)
target_include_directories(SceRtc PUBLIC include)
target_link_libraries(SceRtc PRIVATE module)
