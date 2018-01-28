add_library(SceLibPgf STATIC include/SceLibPgf/exports.h src/ScePgf.cpp)
target_include_directories(SceLibPgf PUBLIC include)
target_link_libraries(SceLibPgf PRIVATE module)
