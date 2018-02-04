add_library(SceNet STATIC include/SceNet/exports.h src/SceNet.cpp)
target_include_directories(SceNet PUBLIC include)
target_link_libraries(SceNet PRIVATE module)