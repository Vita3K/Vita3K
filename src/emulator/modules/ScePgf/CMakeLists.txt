add_library(ScePgf STATIC include/ScePgf/exports.h src/ScePgf.cpp)
target_include_directories(ScePgf PUBLIC include)
target_link_libraries(ScePgf PRIVATE module)
