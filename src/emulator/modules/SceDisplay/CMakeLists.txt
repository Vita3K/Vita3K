add_library(SceDisplay STATIC include/SceDisplay/exports.h src/SceDisplay.cpp src/SceDisplayUser.cpp)
target_include_directories(SceDisplay PUBLIC include)
target_link_libraries(SceDisplay PRIVATE module)
