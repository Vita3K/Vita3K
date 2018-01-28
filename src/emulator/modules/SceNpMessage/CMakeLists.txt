add_library(SceNpMessage STATIC include/SceNpMessage/exports.h src/SceNpMessage.cpp)
target_include_directories(SceNpMessage PUBLIC include)
target_link_libraries(SceNpMessage PRIVATE module)
