add_library(SceLibRudp STATIC include/SceLibRudp/exports.h src/SceLibRudp.cpp)
target_include_directories(SceLibRudp PUBLIC include)
target_link_libraries(SceLibRudp PRIVATE module)
