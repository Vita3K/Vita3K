add_library(
    input
    STATIC
    include/input/physical_key.h
    include/input/physical_key_data.inc
    src/physical_key.cpp
)

target_include_directories(input PUBLIC include)
