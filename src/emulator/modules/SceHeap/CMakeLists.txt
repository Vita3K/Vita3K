add_library(SceHeap STATIC include/SceHeap/exports.h src/SceHeap.cpp)
target_include_directories(SceHeap PUBLIC include)
target_link_libraries(SceHeap PRIVATE module)
