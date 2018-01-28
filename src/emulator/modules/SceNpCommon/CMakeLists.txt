add_library(SceNpCommon STATIC include/SceNpCommon/exports.h src/SceNpCommon.cpp)
target_include_directories(SceNpCommon PUBLIC include)
target_link_libraries(SceNpCommon PRIVATE module)
