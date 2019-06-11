add_library(np
    include/np/profile/profile_manager.h
    include/np/trophy/context.h
    include/np/trophy/trp_parser.h
    include/np/common.h
    include/np/functions.h
    include/np/state.h
    src/profile/profile_manager.cpp
    src/trophy/context.cpp
    src/trophy/trp_parser.cpp
    src/init.cpp)

target_include_directories(np PUBLIC include)
target_link_libraries(np PUBLIC yaml-cpp mem)
target_link_libraries(np PRIVATE io util)
