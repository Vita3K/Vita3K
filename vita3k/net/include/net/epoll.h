#pragma once

#include <net/socket.h>

struct EpollSocket {
    unsigned int events;
    SceNetEpollData data;
    std::weak_ptr<Socket> sock;
};

struct Epoll {
    std::map<int, EpollSocket> eventEntries;

    int add(int id, std::weak_ptr<Socket> sock, SceNetEpollEvent *ev);
    int del(int id);
    int mod(int id, SceNetEpollEvent *ev);
    int wait(SceNetEpollEvent *events, int maxevents, int timeout);
};

typedef std::shared_ptr<Epoll> EpollPtr;
