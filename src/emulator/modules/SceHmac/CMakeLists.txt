add_library(SceHmac STATIC include/SceHmac/exports.h src/SceHmac.cpp)
target_include_directories(SceHmac PUBLIC include)
target_link_libraries(SceHmac PRIVATE module)
