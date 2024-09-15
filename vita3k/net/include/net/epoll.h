#pragma once

#include <net/socket.h>

struct EpollSocket {
    unsigned int events;
    SceNetEpollData data;
    abs_socket sock;
};

struct Epoll {
    std::map<int, EpollSocket> eventEntries;

    int add(int id, abs_socket sock, SceNetEpollEvent *ev);
    int del(int id, abs_socket sock, SceNetEpollEvent *ev);
    int mod(int id, abs_socket sock, SceNetEpollEvent *ev);
    int wait(SceNetEpollEvent *events, int maxevents, int timeout);
};

typedef std::shared_ptr<Epoll> EpollPtr;
