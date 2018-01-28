add_library(SceSysmem STATIC include/SceSysmem/exports.h src/SceSysmem.cpp)
target_include_directories(SceSysmem PUBLIC include)
target_link_libraries(SceSysmem PRIVATE module)
