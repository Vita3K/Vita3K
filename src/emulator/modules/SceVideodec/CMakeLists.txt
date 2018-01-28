add_library(SceVideodec STATIC include/SceVideodec/exports.h src/SceVideodecUser.cpp)
target_include_directories(SceVideodec PUBLIC include)
target_link_libraries(SceVideodec PRIVATE module)
