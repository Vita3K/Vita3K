add_library(
    pfsmgr
    STATIC
    include/pfsmgr/functions.h
    include/pfsmgr/modes.h
    include/pfsmgr/mounts.h
    src/modes.cpp
    src/mounts.cpp
    src/names.cpp
    src/pfsmgr.cpp
    src/savedata_session.cpp
    src/savedata_transaction.cpp
)

target_include_directories(pfsmgr PUBLIC include)
target_link_libraries(pfsmgr PRIVATE crypto util)
