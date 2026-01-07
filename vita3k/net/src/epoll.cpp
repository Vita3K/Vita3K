// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <net/epoll.h>

#include <optional>
#include <util/log.h>

int Epoll::add(int id, std::weak_ptr<Socket> sock, SceNetEpollEvent *ev) {
    if (!eventEntries.try_emplace(id, EpollSocket{ ev->events, ev->data, sock }).second) {
        return SCE_NET_ERROR_EEXIST;
    }

    return 0;
}

int Epoll::del(int id) {
    if (eventEntries.erase(id) == 0) {
        return SCE_NET_ERROR_ENOENT;
    }

    return 0;
}

int Epoll::mod(int id, SceNetEpollEvent *ev) {
    auto it = eventEntries.find(id);
    if (it == eventEntries.end()) {
        return SCE_NET_ERROR_ENOENT;
    }

    it->second.events = ev->events;
    it->second.data = ev->data;
    return 0;
}

static std::string event_to_string(unsigned int events) {
    std::string result;
    if (events & SCE_NET_EPOLLIN) {
        result += "EPOLLIN ";
    }
    if (events & SCE_NET_EPOLLOUT) {
        result += "EPOLLOUT ";
    }
    if (events & SCE_NET_EPOLLERR) {
        result += "EPOLLERR ";
    }
    return result.empty() ? "NONE" : result;
}

static void add_event_fd_set(fd_set *set, int *maxFd, abs_socket sock) {
    FD_SET(sock, set);
    if (sock > *maxFd)
        *maxFd = sock;
}

int Epoll::wait(SceNetEpollEvent *events, int maxevents, int timeout_microseconds) {
    fd_set readFds, writeFds, exceptFds;
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&exceptFds);
    int maxFd = 0;

    const auto get_valid_posix_socket = [](const std::weak_ptr<Socket> &weak_sock, int id) -> std::optional<abs_socket> {
        const auto sock = weak_sock.lock();
        if (!sock) {
            LOG_ERROR("Epoll: Invalid socket for id {}", id);
            return std::nullopt;
        }

        const auto posixSocket = std::dynamic_pointer_cast<PosixSocket>(sock);
        if (!posixSocket) {
            LOG_ERROR("Epoll: Not a valid PosixSocket for id {}", id);
            return std::nullopt;
        }

        return posixSocket->sock;
    };

    for (const auto &[id, entry] : eventEntries) {
        //LOG_DEBUG("Epoll: Checking sock {} for events: {}", id, event_to_string(entry.events));
        const auto sock = get_valid_posix_socket(entry.sock, id);
        if (!sock)
            continue;

        if (entry.events & SCE_NET_EPOLLIN)
            add_event_fd_set(&readFds, &maxFd, *sock);
        if (entry.events & SCE_NET_EPOLLOUT)
            add_event_fd_set(&writeFds, &maxFd, *sock);
        if (entry.events & SCE_NET_EPOLLERR)
            add_event_fd_set(&exceptFds, &maxFd, *sock);
    }

    if (maxFd == 0)
        return 0;

    timeval timeout;
    timeout.tv_sec = timeout_microseconds / 1000000;
    timeout.tv_usec = timeout_microseconds % 1000000;
    const auto ret = select(maxFd + 1, &readFds, &writeFds, &exceptFds, &timeout);
    if (ret < 0) {
        const int err = PosixSocket::translate_return_value(ret);
        LOG_ERROR("select error: {}, {}", ret, log_hex(err));
        return err;
    }

    int eventCount = 0;
    for (const auto &[id, entry] : eventEntries) {
        unsigned int eventTypes = 0;
        const auto sock = get_valid_posix_socket(entry.sock, id);
        if (!sock)
            continue;

        if (FD_ISSET(*sock, &readFds))
            eventTypes |= SCE_NET_EPOLLIN;
        if (FD_ISSET(*sock, &writeFds))
            eventTypes |= SCE_NET_EPOLLOUT;
        if (FD_ISSET(*sock, &exceptFds))
            eventTypes |= SCE_NET_EPOLLERR;

        if (eventTypes != 0 && eventCount < maxevents) {
            auto i = eventCount++;

            events[i].events = eventTypes;
            events[i].data = entry.data;
            //LOG_DEBUG("Epoll: Event for sock {}: {}", id, event_to_string(eventTypes));
        }
    }

    //LOG_DEBUG("Epoll: Total events: {}, max events: {}", eventCount, maxevents);
    return eventCount;
}
