add_library(SceHid STATIC include/SceHid/exports.h src/SceHid.cpp)
target_include_directories(SceHid PUBLIC include)
target_link_libraries(SceHid PRIVATE module)
