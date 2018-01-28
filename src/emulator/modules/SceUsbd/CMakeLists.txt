add_library(SceUsbd STATIC include/SceUsbd/exports.h src/SceUsbdForUser.cpp)
target_include_directories(SceUsbd PUBLIC include)
target_link_libraries(SceUsbd PRIVATE module)
