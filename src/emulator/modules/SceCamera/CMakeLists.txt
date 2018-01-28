add_library(SceCamera STATIC include/SceCamera/exports.h src/SceCamera.cpp)
target_include_directories(SceCamera PUBLIC include)
target_link_libraries(SceCamera PRIVATE module)
