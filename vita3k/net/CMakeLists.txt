add_library(
    net
    STATIC
    include/net/epoll.h
    include/net/functions.h
    include/net/state.h
    include/net/types.h
    include/net/socket.h
    src/epoll.cpp
    src/net.cpp
    src/posixsocket.cpp
    src/p2psocket.cpp
)

target_include_directories(net PUBLIC include)
target_link_libraries(net PUBLIC mem)
if (WIN32)
    target_link_libraries(net PRIVATE winsock)
endif()
