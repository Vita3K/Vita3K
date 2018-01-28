add_library(SceNpBasic STATIC include/SceNpBasic/exports.h src/SceNpBasic.cpp)
target_include_directories(SceNpBasic PUBLIC include)
target_link_libraries(SceNpBasic PRIVATE module)
