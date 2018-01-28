add_library(SceDeci4p STATIC include/SceDeci4p/exports.h src/SceDeci4pUserp.cpp)
target_include_directories(SceDeci4p PUBLIC include)
target_link_libraries(SceDeci4p PRIVATE module)
