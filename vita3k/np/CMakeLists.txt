add_library(np
    include/np/trophy/context.h
    include/np/trophy/trp_parser.h
    include/np/common.h
    include/np/functions.h
    include/np/state.h
    src/trophy/context.cpp
    src/trophy/trp_parser.cpp
    src/init.cpp)

target_include_directories(np PUBLIC include)
target_link_libraries(np PUBLIC yaml-cpp mem)
target_link_libraries(np PRIVATE io util pugixml)
