add_library(
    vfs
    STATIC
    include/vfs/appmeta.h
    include/vfs/mounts.h
    src/appmeta.cpp
    src/mounts.cpp
)

target_include_directories(vfs PUBLIC include)
target_link_libraries(vfs PUBLIC pfsmgr io)
target_link_libraries(vfs PRIVATE kernel)
