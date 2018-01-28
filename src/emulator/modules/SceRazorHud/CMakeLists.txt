add_library(SceRazorHud STATIC include/SceRazorHud/exports.h src/SceRazorHud.cpp)
target_include_directories(SceRazorHud PUBLIC include)
target_link_libraries(SceRazorHud PRIVATE module)
