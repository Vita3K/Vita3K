add_library(SceNpTrophy STATIC include/SceNpTrophy/exports.h src/SceNpTrophy.cpp)
target_include_directories(SceNpTrophy PUBLIC include)
target_link_libraries(SceNpTrophy PRIVATE module)
