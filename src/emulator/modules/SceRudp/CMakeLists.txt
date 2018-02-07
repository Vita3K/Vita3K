add_library(SceRudp STATIC include/SceRudp/exports.h src/SceLibRudp.cpp)
target_include_directories(SceRudp PUBLIC include)
target_link_libraries(SceRudp PRIVATE module)
