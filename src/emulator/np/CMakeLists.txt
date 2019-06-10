add_library(np
    include/np/trophy/trp_parser.h
    include/np/state.h
    src/trophy/trp_parser.cpp)

target_include_directories(np PUBLIC include)
