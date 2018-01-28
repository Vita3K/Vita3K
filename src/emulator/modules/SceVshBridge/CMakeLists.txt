add_library(SceVshBridge STATIC include/SceVshBridge/exports.h src/SceVshBridge.cpp)
target_include_directories(SceVshBridge PUBLIC include)
target_link_libraries(SceVshBridge PRIVATE module)
