add_library(
    updater
    STATIC
    include/updater/functions.h
    include/updater/state.h
    src/common.cpp
)

target_include_directories(updater PUBLIC include)
target_link_libraries(updater PUBLIC config fmt)
