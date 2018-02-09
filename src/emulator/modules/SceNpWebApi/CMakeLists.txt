add_library(SceNpWebApi STATIC include/SceNpWebApi/exports.h src/SceNpWebApi.cpp)
target_include_directories(SceNpWebApi PUBLIC include)
target_link_libraries(SceNpWebApi PRIVATE module)
