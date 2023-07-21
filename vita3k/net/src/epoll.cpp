#include <net/epoll.h>

int Epoll::add(int id, abs_socket sock, SceNetEpollEvent *ev) {
    auto it = eventEntries.find(id);
    if (it != eventEntries.end()) {
        return SCE_NET_ERROR_EEXIST;
    }

    eventEntries.emplace(id, EpollSocket{ ev->events, ev->data, sock });

    return 0;
}

int Epoll::del(int id, abs_socket sock, SceNetEpollEvent *ev) {
    if (eventEntries.erase(id) == 0) {
        return SCE_NET_ERROR_ENOENT;
    }

    return 0;
}

int Epoll::mod(int id, abs_socket sock, SceNetEpollEvent *ev) {
    auto it = eventEntries.find(id);
    if (it == eventEntries.end()) {
        return SCE_NET_ERROR_ENOENT;
    }

    it->second.events = ev->events;
    it->second.data = ev->data;
    return 0;
}

static void add_event_fd_set(fd_set *set, int *maxFd, abs_socket sock) {
    FD_SET(sock, set);
#ifndef _WIN32
    if (sock > *maxFd) {
        *maxFd = sock;
    }
#endif
}

int Epoll::wait(SceNetEpollEvent *events, int maxevents, int timeout_microseconds) {
    fd_set readFds, writeFds, exceptFds;
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&exceptFds);
    int maxFd = 0;

    for (auto &pair : eventEntries) {
        if (pair.second.events & SCE_NET_EPOLLIN) {
            add_event_fd_set(&readFds, &maxFd, pair.second.sock);
        }
        if (pair.second.events & SCE_NET_EPOLLOUT) {
            add_event_fd_set(&writeFds, &maxFd, pair.second.sock);
        }
        if (pair.second.events & SCE_NET_EPOLLERR) {
            add_event_fd_set(&exceptFds, &maxFd, pair.second.sock);
        }
    }

    timeval timeout;
    timeout.tv_sec = timeout_microseconds / 1000000;
    timeout.tv_usec = timeout_microseconds % 1000000;
    auto ret = select(maxFd + 1, &readFds, &writeFds, &exceptFds, &timeout);
    if (ret < 0) {
        // TODO: translate error code
        return -1;
    }

    int eventCount = 0;
    for (auto &pair : eventEntries) {
        unsigned int eventTypes = 0;
        if (FD_ISSET(pair.second.sock, &readFds)) {
            eventTypes |= SCE_NET_EPOLLIN;
        }
        if (FD_ISSET(pair.second.sock, &writeFds)) {
            eventTypes |= SCE_NET_EPOLLOUT;
        }
        if (FD_ISSET(pair.second.sock, &exceptFds)) {
            eventTypes |= SCE_NET_EPOLLERR;
        }

        if (eventTypes != 0 && eventCount < maxevents) {
            auto i = eventCount++;

            events[i].events = eventTypes;
            events[i].data = pair.second.data;
        }
    }

    return eventCount;
}
