add_library(SceClipboard STATIC include/SceClipboard/exports.h src/SceClipboard.cpp)
target_include_directories(SceClipboard PUBLIC include)
target_link_libraries(SceClipboard PRIVATE module)
