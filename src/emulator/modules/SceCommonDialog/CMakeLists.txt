add_library(SceCommonDialog STATIC include/SceCommonDialog/exports.h src/SceCommonDialog.cpp)
target_include_directories(SceCommonDialog PUBLIC include)
target_link_libraries(SceCommonDialog PRIVATE module)
