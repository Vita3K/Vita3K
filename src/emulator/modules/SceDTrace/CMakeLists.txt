add_library(SceDTrace STATIC include/SceDTrace/exports.h src/SceDTrace.cpp)
target_include_directories(SceDTrace PUBLIC include)
target_link_libraries(SceDTrace PRIVATE module)
