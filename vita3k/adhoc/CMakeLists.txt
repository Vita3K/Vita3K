add_library(
    adhoc
    STATIC
    include/adhoc/callbacks.h
    include/adhoc/calloutSyncing.h
    include/adhoc/matchingContext.h
    include/adhoc/matchingTarget.h
    include/adhoc/state.h
    include/adhoc/threads.h
    src/callbacks.cpp
    src/calloutSyncing.cpp
    src/matchingContext.cpp
    src/matchingTarget.cpp
    src/state.cpp
    src/threads.cpp
)

target_include_directories(adhoc PUBLIC include)
target_link_libraries(adhoc PRIVATE util kernel net emuenv kernel io)
target_link_libraries(adhoc PUBLIC modules module)
