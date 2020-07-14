#include <net/socket.h>

int P2PSocket::close() {
    return 0;
}

int P2PSocket::listen(int backlog) {
    return 0;
}

SocketPtr P2PSocket::accept(SceNetSockaddr *addr, unsigned int *addrlen) {
    return nullptr;
}

int P2PSocket::connect(const SceNetSockaddr *addr, unsigned int namelen) {
    return 0;
}

int P2PSocket::set_socket_options(int level, int optname, const void *optval, unsigned int optlen) {
    return 0;
}

int P2PSocket::recv_packet(void *buf, unsigned int len, int flags, SceNetSockaddr *from, unsigned int *fromlen) {
    return SCE_NET_ERROR_EAGAIN;
}

int P2PSocket::send_packet(const void *msg, unsigned int len, int flags, const SceNetSockaddr *to, unsigned int tolen) {
    return 0;
}

int P2PSocket::bind(const SceNetSockaddr *addr, unsigned int addrlen) {
    return 0;
}

int P2PSocket::get_socket_address(SceNetSockaddr *name, unsigned int *namelen) {
    return 0;
}
