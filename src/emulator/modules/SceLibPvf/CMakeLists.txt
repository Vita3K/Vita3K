add_library(SceLibPvf STATIC include/SceLibPvf/exports.h src/ScePvf.cpp)
target_include_directories(SceLibPvf PUBLIC include)
target_link_libraries(SceLibPvf PRIVATE module)
