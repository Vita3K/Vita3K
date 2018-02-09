add_library(ScePvf STATIC include/ScePvf/exports.h src/ScePvf.cpp)
target_include_directories(ScePvf PUBLIC include)
target_link_libraries(ScePvf PRIVATE module)
