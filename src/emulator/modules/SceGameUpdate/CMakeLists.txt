add_library(SceGameUpdate STATIC include/SceGameUpdate/exports.h src/SceGameUpdate.cpp)
target_include_directories(SceGameUpdate PUBLIC include)
target_link_libraries(SceGameUpdate PRIVATE module)
