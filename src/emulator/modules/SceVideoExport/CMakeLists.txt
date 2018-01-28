add_library(SceVideoExport STATIC include/SceVideoExport/exports.h src/SceVideoExport.cpp)
target_include_directories(SceVideoExport PUBLIC include)
target_link_libraries(SceVideoExport PRIVATE module)
