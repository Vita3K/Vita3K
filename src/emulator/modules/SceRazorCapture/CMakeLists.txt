add_library(SceRazorCapture STATIC include/SceRazorCapture/exports.h src/SceRazorCapture.cpp)
target_include_directories(SceRazorCapture PUBLIC include)
target_link_libraries(SceRazorCapture PRIVATE module)
