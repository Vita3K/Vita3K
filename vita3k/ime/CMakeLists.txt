add_library(
    ime
    STATIC
    src/active_ime.cpp
    src/ime.cpp
    include/ime/state.h
    include/ime/types.h
    include/ime/functions.h
)

target_include_directories(ime PUBLIC include)
target_link_libraries(ime PRIVATE dialog emuenv mem util)