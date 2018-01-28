add_library(SceSmart STATIC include/SceSmart/exports.h src/SceSmart.cpp)
target_include_directories(SceSmart PUBLIC include)
target_link_libraries(SceSmart PRIVATE module)
