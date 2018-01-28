add_library(SceNpCommerce2 STATIC include/SceNpCommerce2/exports.h src/SceNpCommerce2.cpp)
target_include_directories(SceNpCommerce2 PUBLIC include)
target_link_libraries(SceNpCommerce2 PRIVATE module)
