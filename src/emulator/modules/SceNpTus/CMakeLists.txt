add_library(SceNpTus STATIC include/SceNpTus/exports.h src/SceNpTus.cpp)
target_include_directories(SceNpTus PUBLIC include)
target_link_libraries(SceNpTus PRIVATE module)
