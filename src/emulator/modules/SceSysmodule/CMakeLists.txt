add_library(SceSysmodule STATIC include/SceSysmodule/exports.h src/SceSysmodule.cpp)
target_include_directories(SceSysmodule PUBLIC include)
target_link_libraries(SceSysmodule PRIVATE module)
