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

#include <module/module.h>

#include <kernel/state.h>
#include <net/state.h>
#include <util/find.h>
#include <util/lock_and_find.h>
#include <util/tracy.h>

#include <chrono>
#include <queue>
#include <thread>

TRACY_MODULE_NAME(SceRudp);

enum SceRudpErrorCode {
    SCE_RUDP_SUCCESS = 0,
    SCE_RUDP_ERROR_NOT_INITIALIZED = 0x80770001,
    SCE_RUDP_ERROR_ALREADY_INITIALIZED = 0x80770002,
    SCE_RUDP_ERROR_INVALID_CONTEXT_ID = 0x80770003,
    SCE_RUDP_ERROR_INVALID_ARGUMENT = 0x80770004,
    SCE_RUDP_ERROR_INVALID_OPTION = 0x80770005,
    SCE_RUDP_ERROR_INVALID_MUXMODE = 0x80770006,
    SCE_RUDP_ERROR_MEMORY = 0x80770007,
    SCE_RUDP_ERROR_INTERNAL = 0x80770008,
    SCE_RUDP_ERROR_CONN_RESET = 0x80770009,
    SCE_RUDP_ERROR_CONN_REFUSED = 0x8077000a,
    SCE_RUDP_ERROR_CONN_TIMEOUT = 0x8077000b,
    SCE_RUDP_ERROR_CONN_VERSION_MISMATCH = 0x8077000c,
    SCE_RUDP_ERROR_CONN_TRANSPORT_TYPE_MISMATCH = 0x8077000d,
    SCE_RUDP_ERROR_CONN_QUALITY_LEVEL_MISMATCH = 0x8077000e,
    SCE_RUDP_ERROR_THREAD = 0x8077000f,
    SCE_RUDP_ERROR_THREAD_IN_USE = 0x80770010,
    SCE_RUDP_ERROR_NOT_ACCEPTABLE = 0x80770011,
    SCE_RUDP_ERROR_MSG_TOO_LARGE = 0x80770012,
    SCE_RUDP_ERROR_NOT_BOUND = 0x80770013,
    SCE_RUDP_ERROR_CANCELLED = 0x80770014,
    SCE_RUDP_ERROR_INVALID_VPORT = 0x80770015,
    SCE_RUDP_ERROR_WOULDBLOCK = 0x80770016,
    SCE_RUDP_ERROR_VPORT_IN_USE = 0x80770017,
    SCE_RUDP_ERROR_VPORT_EXHAUSTED = 0x80770018,
    SCE_RUDP_ERROR_INVALID_SOCKET = 0x80770019,
    SCE_RUDP_ERROR_BUFFER_TOO_SMALL = 0x8077001a,
    SCE_RUDP_ERROR_MSG_MALFORMED = 0x8077001b,
    SCE_RUDP_ERROR_ADDR_IN_USE = 0x8077001c,
    SCE_RUDP_ERROR_ALREADY_BOUND = 0x8077001d,
    SCE_RUDP_ERROR_ALREADY_EXISTS = 0x8077001e,
    SCE_RUDP_ERROR_INVALID_POLL_ID = 0x8077001f,
    SCE_RUDP_ERROR_TOO_MANY_CONTEXTS = 0x80770020,
    SCE_RUDP_ERROR_IN_PROGRESS = 0x80770021,
    SCE_RUDP_ERROR_NO_EVENT_HANDLER = 0x80770022,
    SCE_RUDP_ERROR_PAYLOAD_TOO_LARGE = 0x80770023,
    SCE_RUDP_ERROR_END_OF_DATA = 0x80770024,
    SCE_RUDP_ERROR_ALREADY_ESTABLISHED = 0x80770025,
    SCE_RUDP_ERROR_KEEP_ALIVE_FAILURE = 0x80770026
};

enum SceRudpOption : uint32_t {
    SCE_RUDP_OPTION_MAX_PAYLOAD = 1,
    SCE_RUDP_OPTION_SNDBUF = 2,
    SCE_RUDP_OPTION_RCVBUF = 3,
    SCE_RUDP_OPTION_NODELAY = 4,
    SCE_RUDP_OPTION_DELIVERY_CRITICAL = 5,
    SCE_RUDP_OPTION_ORDER_CRITICAL = 6,
    SCE_RUDP_OPTION_NONBLOCK = 7,
    SCE_RUDP_OPTION_STREAM = 8,
    SCE_RUDP_OPTION_CONNECTION_TIMEOUT = 9,
    SCE_RUDP_OPTION_CLOSE_WAIT_TIMEOUT = 10,
    SCE_RUDP_OPTION_AGGREGATION_TIMEOUT = 11,
    SCE_RUDP_OPTION_LAST_ERROR = 14,
    SCE_RUDP_OPTION_READ_TIMEOUT = 15,
    SCE_RUDP_OPTION_WRITE_TIMEOUT = 16,
    SCE_RUDP_OPTION_FLUSH_TIMEOUT = 17,
    SCE_RUDP_OPTION_KEEP_ALIVE_INTERVAL = 18,
    SCE_RUDP_OPTION_KEEP_ALIVE_TIMEOUT = 19,
};

static const std::map<SceRudpOption, int32_t> default_options = {
    { SCE_RUDP_OPTION_MAX_PAYLOAD, 1346 }, // Maximum payload size of RUDP segment
    { SCE_RUDP_OPTION_SNDBUF, 65536 }, // Send buffer size
    { SCE_RUDP_OPTION_RCVBUF, 65536 }, // Receive buffer size
    { SCE_RUDP_OPTION_NODELAY, 0 }, // Message aggregation for context
    { SCE_RUDP_OPTION_DELIVERY_CRITICAL, 1 }, // Delivery Critical (DC) option
    { SCE_RUDP_OPTION_ORDER_CRITICAL, 1 }, // Order Critical (OC) option
    { SCE_RUDP_OPTION_NONBLOCK, 1 }, // Nonblocking mode
    { SCE_RUDP_OPTION_STREAM, 0 }, // Transport type
    { SCE_RUDP_OPTION_CONNECTION_TIMEOUT, 60000 }, // Connection timeout
    { SCE_RUDP_OPTION_CLOSE_WAIT_TIMEOUT, 0 }, // Close-wait timeout
    { SCE_RUDP_OPTION_AGGREGATION_TIMEOUT, 30 }, // Aggregation timeout
    { SCE_RUDP_OPTION_LAST_ERROR, SCE_RUDP_SUCCESS },
    { SCE_RUDP_OPTION_READ_TIMEOUT, 0 }, // Read timeout
    { SCE_RUDP_OPTION_WRITE_TIMEOUT, 0 }, // Write timeout
    { SCE_RUDP_OPTION_FLUSH_TIMEOUT, 0 }, // Flush timeout
    { SCE_RUDP_OPTION_KEEP_ALIVE_INTERVAL, 0 }, // Keep-alive interval
    { SCE_RUDP_OPTION_KEEP_ALIVE_TIMEOUT, 0 } // Keep-alive timeout
};

enum SceRudpState : uint32_t {
    SCE_RUDP_STATE_IDLE = 0,
    SCE_RUDP_STATE_CLOSED = 1,
    SCE_RUDP_STATE_SYN_SENT = 2,
    SCE_RUDP_STATE_SYN_RCVD = 3,
    SCE_RUDP_STATE_ESTABLISHED = 4,
    SCE_RUDP_STATE_CLOSE_WAIT = 5,
};

enum SceRudpMsgFlags {
    SCE_RUDP_MSG_DONTWAIT = 0x01,
    SCE_RUDP_MSG_LATENCY_CRITICAL = 0x08,
    SCE_RUDP_MSG_ALIGN_32 = 0x10,
    SCE_RUDP_MSG_ALIGN_64 = 0x20,
    SCE_RUDP_MSG_WITH_TX_TIMESTAMP = 0x40
};

enum RudpPollEventFlags : uint32_t {
    SCE_RUDP_POLL_EV_READ = 0x0001, // Data can be read
    SCE_RUDP_POLL_EV_WRITE = 0x0002, // Data can be written
    SCE_RUDP_POLL_EV_FLUSH = 0x0004, // Flush completed
    SCE_RUDP_POLL_EV_ERROR = 0x0008, // Error occurred
};

enum SceRudpPollOp {
    SCE_RUDP_POLL_OP_ADD = 1, // Add target contexts
    SCE_RUDP_POLL_OP_MODIFY = 2, // Modify target contexts
    SCE_RUDP_POLL_OP_REMOVE = 3, // Delete target contexts
};

typedef SceUInt32 SceNetSocklen_t;
typedef uint64_t SceRudpUsec;

typedef void (*SceRudpContextEventHandler)(
    int ctxId,
    int eventId,
    int errorCode,
    void *arg);

typedef int (*SceRudpEventHandler)(
    int eventId,
    int soc,
    uint8_t const *data,
    uint32_t dataLen,
    struct SceNetSockaddr const *addr,
    SceNetSocklen_t addrLen,
    void *arg);

struct SceRudpReadInfo {
    uint8_t size;
    uint8_t retransmissionCount;
    uint16_t retransmissionDelay;
    uint8_t retransmissionDelay2;
    uint8_t flags;
    uint16_t sequenceNumber;
    uint32_t timestamp;
};

struct SceRudpStatus {
    uint64_t sentUdpBytes;
    uint64_t rcvdUdpBytes;
    uint32_t sentUdpPackets;
    uint32_t rcvdUdpPackets;
    uint64_t sentUserBytes;
    uint32_t sentUserPackets;
    uint64_t rcvdUserBytes;
    uint32_t rcvdUserPackets;
    uint32_t sentLatencyCriticalPackets;
    uint32_t rcvdLatencyCriticalPackets;
    uint32_t sentSynPackets;
    uint32_t rcvdSynPackets;
    uint32_t sentUsrPackets;
    uint32_t rcvdUsrPackets;
    uint32_t sentPrbPackets;
    uint32_t rcvdPrbPackets;
    uint32_t sentRstPackets;
    uint32_t rcvdRstPackets;
    uint32_t lostPackets;
    uint32_t retransmittedPackets;
    uint32_t reorderedPackets;
    uint32_t currentContexts;
    uint64_t sentQualityLevel1Bytes;
    uint64_t rcvdQualityLevel1Bytes;
    uint32_t sentQualityLevel1Packets;
    uint32_t rcvdQualityLevel1Packets;
    uint64_t sentQualityLevel2Bytes;
    uint64_t rcvdQualityLevel2Bytes;
    uint32_t sentQualityLevel2Packets;
    uint32_t rcvdQualityLevel2Packets;
    uint64_t sentQualityLevel3Bytes;
    uint64_t rcvdQualityLevel3Bytes;
    uint32_t sentQualityLevel3Packets;
    uint32_t rcvdQualityLevel3Packets;
    uint64_t sentQualityLevel4Bytes;
    uint64_t rcvdQualityLevel4Bytes;
    uint32_t sentQualityLevel4Packets;
    uint32_t rcvdQualityLevel4Packets;
    uint32_t allocs;
    uint32_t frees;
    uint32_t memCurrent;
    uint32_t memPeak;
    uint32_t establishedConnections;
    uint32_t failedConnections;
    uint32_t failedConnectionsReset;
    uint32_t failedConnectionsRefused;
    uint32_t failedConnectionsTimeout;
    uint32_t failedConnectionsVersionMismatch;
    uint32_t failedConnectionsTransportTypeMismatch;
    uint32_t failedConnectionsQualityLevelMismatch;
};

struct SceRudpContextStatus {
    uint32_t state;
    int parentId = 0;
    uint32_t children = 0;
    uint32_t lostPackets;
    uint32_t sentPackets;
    uint32_t rcvdPackets;
    uint64_t sentBytes;
    uint64_t rcvdBytes;
    uint32_t retransmissions;
    uint32_t rtt;
};

enum RudpSegmentType : uint8_t {
    RUDP_SEGMENT_TYPE_SYN = 0x01, // SYN segment
    RUDP_SEGMENT_TYPE_SYNCACK = 0x02, // SYNCACK segment
    RUDP_SEGMENT_TYPE_DATA = 0x03, // DATA segment
    RUDP_SEGMENT_TYPE_ACK = 0x04, // ACK segment
    RUDP_SEGMENT_TYPE_RST = 0x05, // RST segment
};

static std::string rudp_segment_type_to_string(uint32_t type) {
    switch (type) {
    case RUDP_SEGMENT_TYPE_SYN:
        return "SYN";
    case RUDP_SEGMENT_TYPE_SYNCACK:
        return "SYNCACK";
    case RUDP_SEGMENT_TYPE_DATA:
        return "DATA";
    case RUDP_SEGMENT_TYPE_ACK:
        return "ACK";
    case RUDP_SEGMENT_TYPE_RST:
        return "RST";
    default:
        return "UNKNOWN";
    }
}

struct RudpSegmentHeader {
    uint8_t type; // Type of the segment (SYN, SYNCACK, DATA, ACK, RST)
    uint32_t sequence_number = 0; // Sequence number of the segment
    uint8_t index = 0; // Index of the segment in the sequence
    uint8_t count = 0; // Total number of segments in the sequence
    uint32_t timestamp; // Timestamp of the segment (in ticks)
    uint8_t retransmission_count = 0; // Number of retransmissions for this segment
    uint32_t payload_len = 0; // Len of the payload in bytes
};

struct RudpSegment {
    RudpSegmentHeader header;
    std::vector<uint8_t> payload;
};

struct SegmentBuffer {
    uint16_t segment_count = 0;
    std::map<uint16_t, std::vector<uint8_t>> segments;
    uint64_t timestamp;
};

struct RudpContext {
    // Receive buffer
    std::queue<RudpSegment> recv_buffer;
    int64_t current_recv_bytes = 0;
    uint16_t last_recv_seq = 0;

    std::unordered_map<uint16_t, SegmentBuffer> segment_buffers;

    // Send buffer
    std::queue<std::vector<uint8_t>> send_buffer;
    int64_t current_send_bytes = 0;
    uint16_t last_send_seq = 0;

    // Aggregation
    std::vector<uint8_t> aggregation_buffer;
    uint32_t last_aggregation_time = 0;

    uint64_t established_timestamp = 0;

    bool is_handled_by_poll = false;
    bool is_readable = false;

    // Options
    std::map<SceRudpOption, int32_t> options = default_options;

    uint64_t timestamp_syn_sent = 0;
    uint64_t timestamp_start_connect = 0;

    uint16_t vport = 0;
    int bound_socket_id = 0;
    uint8_t mux_mode = 1;

    bool bound = false;

    uint16_t max_segment_size = 1410;

    SceNetSockaddr peer_addr{};
    SceNetSocklen_t peer_addr_len = 0;

    // State
    int ctx_id;
    SceRudpContextEventHandler handler;
    void *arg;
    SceRudpContextStatus status{};
};

struct SceRudpPollEvent {
    int ctxId;
    uint16_t reqEvents;
    uint16_t rtnEvents;
};

struct RudpPoll {
    size_t max_size;
    std::map<int, uint16_t> entries;
};

struct RudpState {
    bool inited = false;
    int next_id = 0;
    void *pool = nullptr;
    uint32_t pool_size = 0;

    SceRudpEventHandler event_handler = nullptr;
    void *event_handler_arg = nullptr;

    bool thread_started = false;
    std::thread io_write_thread;
    std::thread io_read_thread;

    SceRudpStatus status{};

    std::map<int, std::shared_ptr<RudpContext>> contexts;
    std::map<int, std::shared_ptr<RudpPoll>> polls;

    int next_poll_id = 0;
    std::mutex mutex;
    std::condition_variable io_cv;
    std::condition_variable poll_cv;
    std::condition_variable send_cv;
    std::condition_variable recv_cv;
    std::mutex io_mutex;
    std::mutex poll_mutex;
    std::mutex recv_mutex;
    std::mutex send_mutex;

    uint16_t max_segment_size = 1410;
};

static RudpState rudp;
static bool log_rudp = true;

static uint32_t rudp_get_ticks(EmuEnvState &emuenv) {
    const uint64_t elapsed_us = rtc_get_ticks(emuenv.kernel.base_tick.tick) - emuenv.kernel.start_tick;
    return static_cast<uint32_t>(elapsed_us / 1000); // convert microseconds to milliseconds
}

#define RUDP_ERROR_CASE(net_err, rudp_err) \
    case SCE_NET_ERROR_##net_err:          \
        return SCE_RUDP_ERROR##rudp_err;

static int translate_return_value(int retval) {
    if (retval >= 0)
        return retval;
#ifdef _WIN32
    switch (WSAGetLastError()) {
#else
    switch (errno) {
#endif
        RUDP_ERROR_CASE(ENOMEM, _MEMORY)
        RUDP_ERROR_CASE(EINVAL, _INVALID_ARGUMENT)
        RUDP_ERROR_CASE(ECANCELED, _CANCELLED)
        RUDP_ERROR_CASE(ECONNRESET, _CONN_RESET)
        RUDP_ERROR_CASE(ECONNREFUSED, _CONN_REFUSED)
        RUDP_ERROR_CASE(ETIMEDOUT, _CONN_TIMEOUT)
        RUDP_ERROR_CASE(EALREADY, _ALREADY_ESTABLISHED)
        RUDP_ERROR_CASE(EINPROGRESS, _IN_PROGRESS)
        RUDP_ERROR_CASE(EMSGSIZE, _MSG_TOO_LARGE)
        RUDP_ERROR_CASE(EADDRINUSE, _ADDR_IN_USE)
        RUDP_ERROR_CASE(ENOTSOCK, _INVALID_SOCKET)
        RUDP_ERROR_CASE(EWOULDBLOCK, _WOULDBLOCK)

    default:
        LOG_ERROR("Error: {}", log_hex(retval));
        return SCE_RUDP_ERROR_INTERNAL;
    }
}

static const std::string rudp_option_to_string(int option) {
    switch (option) {
    case SCE_RUDP_OPTION_MAX_PAYLOAD:
        return "SCE_RUDP_OPTION_MAX_PAYLOAD";
    case SCE_RUDP_OPTION_SNDBUF:
        return "SCE_RUDP_OPTION_SNDBUF";
    case SCE_RUDP_OPTION_RCVBUF:
        return "SCE_RUDP_OPTION_RCVBUF";
    case SCE_RUDP_OPTION_NODELAY:
        return "SCE_RUDP_OPTION_NODELAY";
    case SCE_RUDP_OPTION_DELIVERY_CRITICAL:
        return "SCE_RUDP_OPTION_DELIVERY_CRITICAL";
    case SCE_RUDP_OPTION_ORDER_CRITICAL:
        return "SCE_RUDP_OPTION_ORDER_CRITICAL";
    case SCE_RUDP_OPTION_NONBLOCK:
        return "SCE_RUDP_OPTION_NONBLOCK";
    case SCE_RUDP_OPTION_STREAM:
        return "SCE_RUDP_OPTION_STREAM";
    case SCE_RUDP_OPTION_CONNECTION_TIMEOUT:
        return "SCE_RUDP_OPTION_CONNECTION_TIMEOUT";
    case SCE_RUDP_OPTION_CLOSE_WAIT_TIMEOUT:
        return "SCE_RUDP_OPTION_CLOSE_WAIT_TIMEOUT";
    case SCE_RUDP_OPTION_AGGREGATION_TIMEOUT:
        return "SCE_RUDP_OPTION_AGGREGATION_TIMEOUT";
    case SCE_RUDP_OPTION_LAST_ERROR:
        return "SCE_RUDP_OPTION_LAST_ERROR";
    case SCE_RUDP_OPTION_READ_TIMEOUT:
        return "SCE_RUDP_OPTION_READ_TIMEOUT";
    case SCE_RUDP_OPTION_WRITE_TIMEOUT:
        return "SCE_RUDP_OPTION_WRITE_TIMEOUT";
    case SCE_RUDP_OPTION_FLUSH_TIMEOUT:
        return "SCE_RUDP_OPTION_FLUSH_TIMEOUT";
    case SCE_RUDP_OPTION_KEEP_ALIVE_INTERVAL:
        return "SCE_RUDP_OPTION_KEEP_ALIVE_INTERVAL";
    case SCE_RUDP_OPTION_KEEP_ALIVE_TIMEOUT:
        return "SCE_RUDP_OPTION_KEEP_ALIVE_TIMEOUT";
    default:
        return std::to_string(option);
    }
}

static std::string rudp_state_to_string(uint32_t state) {
    switch (state) {
    case SCE_RUDP_STATE_IDLE:
        return "SCE_RUDP_STATE_IDLE";
    case SCE_RUDP_STATE_CLOSED:
        return "SCE_RUDP_STATE_CLOSED";
    case SCE_RUDP_STATE_SYN_SENT:
        return "SCE_RUDP_STATE_SYN_SENT";
    case SCE_RUDP_STATE_SYN_RCVD:
        return "SCE_RUDP_STATE_SYN_RCVD";
    case SCE_RUDP_STATE_ESTABLISHED:
        return "SCE_RUDP_STATE_ESTABLISHED";
    case SCE_RUDP_STATE_CLOSE_WAIT:
        return "SCE_RUDP_STATE_CLOSE_WAIT";
    default:
        return "UNKNOWN STATE: " + std::to_string(state);
    }
}

static std::string rudp_poll_op_to_string(int op) {
    switch (op) {
    case SCE_RUDP_POLL_OP_ADD:
        return "SCE_RUDP_POLL_OP_ADD";
    case SCE_RUDP_POLL_OP_MODIFY:
        return "SCE_RUDP_POLL_OP_MODIFY";
    case SCE_RUDP_POLL_OP_REMOVE:
        return "SCE_RUDP_POLL_OP_REMOVE";
    default:
        return "UNKNOWN OP: " + std::to_string(op);
    }
}

static std::string rudp_poll_event_flags_to_string(uint16_t flags) {
    std::string result;
    if (flags & SCE_RUDP_POLL_EV_READ)
        result += "READ ";
    if (flags & SCE_RUDP_POLL_EV_WRITE)
        result += "WRITE ";
    if (flags & SCE_RUDP_POLL_EV_FLUSH)
        result += "FLUSH ";
    if (flags & SCE_RUDP_POLL_EV_ERROR)
        result += "ERROR ";
    return result.empty() ? "NONE" : result;
}

static int rudp_set_last_error(RudpContext *ctx, int error) {
    ctx->options[SCE_RUDP_OPTION_LAST_ERROR] = error;
    LOG_ERROR("RUDP last error set: {}", log_hex(error));
    return error;
}

static std::vector<uint8_t> convert_segment_to_buffer(const RudpSegment &segment) {
    // Convert RudpSegment to a byte buffer
    std::vector<uint8_t> buffer;
    const auto segment_header_ptr = reinterpret_cast<const uint8_t *>(&segment.header);
    buffer.insert(buffer.end(), segment_header_ptr, segment_header_ptr + sizeof(RudpSegmentHeader));
    buffer.insert(buffer.end(), segment.payload.begin(), segment.payload.end());

    return buffer;
}

static void flush_buffer(RudpContext *ctx, std::vector<uint8_t> &buffer) {
    std::lock_guard<std::mutex> lock(rudp.send_mutex);
    if (!buffer.empty()) {
        LOG_INFO("Flushed segments to send buffer, size: {}", buffer.size());
        ctx->send_buffer.emplace(std::move(buffer));
    } else {
        LOG_WARN("Attempted to flush empty segment buffer.");
    }
}

static void rudp_io_write_thread(EmuEnvState &emuenv) {
    LOG_INFO("RUDP I/O write thread started.");
    while (rudp.inited) {
        std::unique_lock<std::mutex> io_lock(rudp.io_mutex);
        rudp.io_cv.wait(io_lock, [&] { return !rudp.contexts.empty() || !rudp.inited; });
        io_lock.unlock();

        if (!rudp.inited)
            break;

        // LOG_INFO("[sceRudpEnableInternalIOThread] RUDP I/O thread woke up.");
        for (auto &[ctx_id, ctx] : rudp.contexts) {
            if (!ctx || (ctx->status.state == SCE_RUDP_STATE_IDLE))
                continue;

            auto socket = lock_and_find(ctx->bound_socket_id, emuenv.net.socks, emuenv.kernel.mutex);
            if (!socket)
                continue;

            if (!ctx->aggregation_buffer.empty() && (ctx->last_aggregation_time + ctx->options[SCE_RUDP_OPTION_AGGREGATION_TIMEOUT]) <= rudp_get_ticks(emuenv))
                flush_buffer(ctx.get(), ctx->aggregation_buffer);

            // Send
            if (!ctx->send_buffer.empty()) {
                auto &segments = ctx->send_buffer.front();
                const int res = socket->send_packet(segments.data(), segments.size(), 0,
                    &ctx->peer_addr, ctx->peer_addr_len);
                if (res < 0) {
                    LOG_ERROR("send error: {}", log_hex(translate_return_value(res)));
                    ctx->options[SCE_RUDP_OPTION_LAST_ERROR] = translate_return_value(res);
                    continue;
                }

                {
                    std::lock_guard lock(rudp.mutex);
                    size_t offset = 0;
                    while ((offset + sizeof(RudpSegmentHeader)) <= res) {
                        const auto segment_header = reinterpret_cast<const RudpSegmentHeader *>(segments.data() + offset);
                        const size_t expected_size = sizeof(RudpSegmentHeader) + segment_header->payload_len;

                        LOG_INFO("Success Sent: {} bytes - type: {}, seq: {}, index: {}, count: {}, size: {}, expected_size: {}",
                            res, rudp_segment_type_to_string(segment_header->type), segment_header->sequence_number,
                            segment_header->index, segment_header->count, segment_header->payload_len, expected_size);

                        if (segment_header->type == RUDP_SEGMENT_TYPE_DATA) {
                            const auto payload_len = segment_header->payload_len;
                            ctx->status.sentBytes += payload_len;
                            rudp.status.sentUserBytes += payload_len;
                            ctx->current_send_bytes -= payload_len;
                        }

                        offset += expected_size;
                    }

                    rudp.status.sentUserPackets++;
                    ctx->status.sentPackets++;

                    ctx->send_buffer.pop();
                }
            }
        }
    }

    LOG_INFO("[sceRudpEnableInternalIOThread] RUDP I/O thread write exiting.");
}

static void event_fd_set(fd_set *set, int *maxFd, abs_socket sock) {
    FD_SET(sock, set);
#ifndef _WIN32
    if (sock > *maxFd) {
        *maxFd = sock;
    }
#endif
}

struct RudpPollSelect {
    int ctx_id;
    uint16_t events;
    abs_socket sock;
};

static int rudp_poll_select(const std::vector<RudpPollSelect> &poll, SceRudpUsec timeout, std::map<uint32_t, uint16_t> &events_type) {
    fd_set read_fds, write_fds, except_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);
    int max_fd = 0;

    {
        std::lock_guard lock(rudp.poll_mutex);
        for (auto &[_, events, sock] : poll) {
            if (events & SCE_RUDP_POLL_EV_READ)
                event_fd_set(&read_fds, &max_fd, sock);
            if (events & SCE_RUDP_POLL_EV_WRITE)
                event_fd_set(&write_fds, &max_fd, sock);
            if (events & SCE_RUDP_POLL_EV_ERROR)
                event_fd_set(&except_fds, &max_fd, sock);
        }
    }

    timeval tv{
        .tv_sec = static_cast<long>(timeout / 1000000),
        .tv_usec = static_cast<long>(timeout % 1000000)
    };

    auto ret = select(max_fd + 1, &read_fds, &write_fds, &except_fds, &tv);
    if (ret < 0) {
        LOG_ERROR("select error: {}", translate_return_value(ret));
        return -1;
    }

    if (ret == 0)
        return 0; // Timeout

    for (auto &[ctx_id, sock_events, sock] : poll) {
        auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
        if (!ctx) {
            LOG_ERROR("Context not found: ctx_id={}", ctx_id);
            continue;
        }
        uint16_t events = 0;
        if (FD_ISSET(sock, &read_fds)) {
            events |= SCE_RUDP_POLL_EV_READ;
            {
                std::lock_guard lock(rudp.poll_mutex);
                ctx->is_readable = true;
            }
        }
        if (FD_ISSET(sock, &write_fds)) {
            events |= SCE_RUDP_POLL_EV_WRITE;
        }
        if (FD_ISSET(sock, &except_fds)) {
            events |= SCE_RUDP_POLL_EV_ERROR;
        }
        events_type.emplace(ctx_id, events);
    }

    return ret; // Number of sockets with events
}

static void rudp_io_read_thread(EmuEnvState &emuenv) {
    LOG_INFO("RUDP I/O thread read started.");
    while (rudp.inited) {
        std::unique_lock<std::mutex> io_lock(rudp.io_mutex);
        rudp.io_cv.wait(io_lock, [&] { return !rudp.contexts.empty() || !rudp.inited; });
        io_lock.unlock();

        if (!rudp.inited)
            break;

        // LOG_INFO("[sceRudpEnableInternalIOThread] RUDP I/O thread woke up.");
        for (auto &[ctx_id, ctx] : rudp.contexts) {
            if (!ctx || (ctx->status.state == SCE_RUDP_STATE_IDLE))
                continue;

            auto socket = lock_and_find(ctx->bound_socket_id, emuenv.net.socks, emuenv.kernel.mutex);
            if (!socket)
                continue;

            if (!ctx->is_handled_by_poll) {
                // If not handled by poll, we need to wait for data to be available
                auto sock = std::dynamic_pointer_cast<PosixSocket>(socket)->sock;
                std::map<uint32_t, uint16_t> events_type;
                rudp_poll_select({ { ctx_id, SCE_RUDP_POLL_EV_READ, sock } }, static_cast<SceRudpUsec>(ctx->options[SCE_RUDP_OPTION_READ_TIMEOUT] * 1000), events_type);
            }

            {
                std::unique_lock<std::mutex> recv_lock(rudp.poll_mutex);
                // Check if the context is still readable after select
                if (!ctx->is_readable)
                    continue;
                else
                    ctx->is_readable = false; // Reset for next iteration
            }

            LOG_DEBUG_IF(log_rudp, "Try Recv: {} bytes", ctx->options[SCE_RUDP_OPTION_RCVBUF]);
            std::vector<uint8_t> recv_tmp(ctx->options[SCE_RUDP_OPTION_RCVBUF]);
            auto peer_addr = ctx->peer_addr;
            const auto res = socket->recv_packet(recv_tmp.data(), recv_tmp.size(), 0,
                &peer_addr, &ctx->peer_addr_len);
            if (res > 0) {
                if (res < sizeof(RudpSegmentHeader)) {
                    LOG_ERROR("Received packet too small: {} bytes", res);
                    continue;
                }

                LOG_DEBUG_IF(log_rudp, "Received packet: {} bytes", res);

                std::vector<RudpSegment> segments;
                size_t offset = 0;
                while ((offset + sizeof(RudpSegmentHeader)) <= res) {
                    const auto header = reinterpret_cast<const RudpSegmentHeader *>(recv_tmp.data() + offset);
                    const size_t expected_size = sizeof(RudpSegmentHeader) + header->payload_len;

                    if ((offset + expected_size) > res) {
                        LOG_WARN("Truncated RUDP segment in aggregated packet (expected {}, available {})", expected_size, res - offset);
                        break;
                    }

                    const auto payload_offset = recv_tmp.begin() + offset + sizeof(RudpSegmentHeader);
                    RudpSegment segment{
                        .header = *header,
                        .payload = std::vector<uint8_t>(payload_offset, payload_offset + header->payload_len)
                    };

                    LOG_INFO("Received fragment: type={}, seq={}, index={}, count={}, size={}, expected_size={}",
                        rudp_segment_type_to_string(segment.header.type), segment.header.sequence_number, segment.header.index,
                        segment.header.count, segment.payload.size(), expected_size);

                    segments.emplace_back(std::move(segment));
                    offset += expected_size;
                    ctx->last_recv_seq = segment.header.sequence_number;
                }

                for (const auto &segment : segments) {
                    switch (segment.header.type) {
                    case RUDP_SEGMENT_TYPE_SYN:
                        if ((ctx->status.state == SCE_RUDP_STATE_CLOSED) || (ctx->status.state == SCE_RUDP_STATE_SYN_SENT)) {
                            ctx->status.state = SCE_RUDP_STATE_SYN_RCVD;

                            // Send a SYNCACK response
                            RudpSegment synack_segment{
                                .header = {
                                    .type = RUDP_SEGMENT_TYPE_SYNCACK,
                                    .sequence_number = ++ctx->last_send_seq,
                                    .timestamp = rudp_get_ticks(emuenv),
                                },
                                .payload = {}
                            };

                            LOG_INFO("Enqueued SYNCACK response (seq: {})", synack_segment.header.sequence_number);
                            auto synack_buffer = convert_segment_to_buffer(std::move(synack_segment));
                            flush_buffer(ctx.get(), synack_buffer);
                        } else
                            LOG_WARN("Received SYN while in unexpected state: {}", ctx->status.state);

                        rudp.status.rcvdSynPackets++;

                        break;
                    case RUDP_SEGMENT_TYPE_SYNCACK:
                        if ((ctx->status.state == SCE_RUDP_STATE_SYN_SENT) || (ctx->status.state == SCE_RUDP_STATE_SYN_RCVD)) {
                            ctx->status.state = SCE_RUDP_STATE_ESTABLISHED;
                            ctx->established_timestamp = rudp_get_ticks(emuenv);

                            LOG_INFO("Connection successfully established.");

                            // Send ACK to confirm SYNCACK and complete handshake
                            RudpSegment ack_segment{
                                .header = {
                                    .type = RUDP_SEGMENT_TYPE_ACK,
                                    .sequence_number = ++ctx->last_send_seq,
                                    .index = 0,
                                    .count = 1,
                                    .timestamp = rudp_get_ticks(emuenv),
                                    .retransmission_count = 0,
                                    .payload_len = 0 // No payload for ACK
                                },
                                .payload = {}
                            };

                            // Enqueue ACK segment to send buffer
                            LOG_INFO("Enqueued ACK response (seq: {})", ack_segment.header.sequence_number);
                            auto ack_buffer = convert_segment_to_buffer(std::move(ack_segment));
                            flush_buffer(ctx.get(), ack_buffer);
                        } else {
                            LOG_WARN("Received SYNCACK while in unexpected state: {}", ctx->status.state);
                        }
                        break;
                    case RUDP_SEGMENT_TYPE_DATA:
                        if (ctx->status.state != SCE_RUDP_STATE_ESTABLISHED) {
                            LOG_WARN("Received DATA segment while in unexpected state: {}", ctx->status.state);
                            continue;
                        }

                        if (segment.header.count > 1) {
                            // Look if we already have a buffer for this sequence_number
                            auto &buffer = ctx->segment_buffers[segment.header.sequence_number];

                            // If this is the first fragment received, initialize the buffer
                            if (buffer.segments.empty()) {
                                buffer.segment_count = segment.header.count;
                                buffer.timestamp = rudp_get_ticks(emuenv);
                            }

                            // Store the fragment payload
                            buffer.segments[segment.header.index] = segment.payload;

                            // Check if we received all fragments
                            if (buffer.segments.size() == buffer.segment_count) {
                                // Reassemble
                                RudpSegment full_message{
                                    .header = {
                                        .type = RUDP_SEGMENT_TYPE_DATA,
                                        .sequence_number = segment.header.sequence_number,
                                        .index = 0,
                                        .count = 1,
                                        .timestamp = segment.header.timestamp },
                                    .payload = {}
                                };

                                for (auto &frag : buffer.segments)
                                    full_message.payload.insert(full_message.payload.end(), frag.second.begin(), frag.second.end());

                                const auto full_payload_len = full_message.payload.size();
                                full_message.header.payload_len = full_payload_len;

                                // Pass full_message to upper layer
                                ctx->recv_buffer.emplace(std::move(full_message));

                                ctx->current_recv_bytes += full_payload_len;
                                ctx->status.rcvdBytes += full_payload_len;
                                rudp.status.rcvdUserBytes += full_payload_len;
                                LOG_INFO("Reassembled and pushed full message (seq: {}, size: {})",
                                    full_message.header.sequence_number, full_payload_len);

                                // Remove from map
                                ctx->segment_buffers.erase(segment.header.sequence_number);
                            }
                        } else {
                            const auto payload_len = segment.header.payload_len;
                            ctx->recv_buffer.push(std::move(segment));
                            ctx->current_recv_bytes += payload_len;
                            ctx->status.rcvdBytes += payload_len;
                            ctx->status.rcvdPackets++;
                            rudp.status.rcvdUserBytes += payload_len;
                            rudp.status.rcvdUserPackets++;
                        }
                        ctx->status.rcvdPackets++;
                        rudp.status.rcvdUserPackets++;

                        rudp.recv_cv.notify_all();

                        break;
                    case RUDP_SEGMENT_TYPE_ACK:
                        // If we are in SYN_RCVD state, this means the connection was established after receiving the ACK.
                        switch (ctx->status.state) {
                        case SCE_RUDP_STATE_SYN_RCVD:
                            ctx->status.state = SCE_RUDP_STATE_ESTABLISHED;
                            ctx->established_timestamp = rudp_get_ticks(emuenv);

                            LOG_INFO("Connection successfully established (after ACK).");
                            break;
                        default:
                            LOG_WARN("Received ACK while in unexpected state: {}", ctx->status.state);
                            break;
                        }
                        break;
                    case RUDP_SEGMENT_TYPE_RST:
                        LOG_WARN("Handling RST segment (connection reset)");
                        // TODO: close connection
                        break;
                    default:
                        LOG_WARN("Unknown RUDP segment type: {}", segment.header.type);
                        break;
                    }
                }
            } else if (res < 0) {
                LOG_ERROR("recv error: {}", res);
            }
        }
    }

    LOG_INFO("[sceRudpEnableInternalIOThread] RUDP I/O Read thread exiting.");
}

EXPORT(int, sceRudpActivate, int ctx_id, const SceNetSockaddr *to, SceNetSocklen_t to_len) {
    TRACY_FUNC(sceRudpActivate, ctx_id, to, to_len);
    LOG_INFO("sceRudpActivate(ctx_id={}, to_len={})", ctx_id, to_len);

    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (!to || to_len > sizeof(SceNetSockaddrIn) || ctx_id < 0)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
    if (!ctx)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);

    if (!ctx->bound)
        return rudp_set_last_error(ctx.get(), SCE_RUDP_ERROR_NOT_BOUND);

    if (ctx->status.state != SCE_RUDP_STATE_IDLE)
        return rudp_set_last_error(ctx.get(), SCE_RUDP_ERROR_NOT_ACCEPTABLE);

    // Check for duplicate peer address (ADDR_IN_USE, if same socket/peer already used)
    for (const auto &[id, other_ctx] : rudp.contexts) {
        if (other_ctx.get() != ctx.get() && other_ctx->bound_socket_id == ctx->bound_socket_id && other_ctx->peer_addr_len == to_len && memcmp(&other_ctx->peer_addr, to, to_len) == 0)
            return rudp_set_last_error(ctx.get(), SCE_RUDP_ERROR_ADDR_IN_USE);
    }

    memcpy(&ctx->peer_addr, to, to_len);
    ctx->peer_addr_len = to_len;

    ctx->status.state = SCE_RUDP_STATE_CLOSED;
    rudp_set_last_error(ctx.get(), SCE_RUDP_SUCCESS);

    const auto nonblock = ctx->options[SCE_RUDP_OPTION_NONBLOCK];
    if (nonblock)
        return SCE_RUDP_SUCCESS;

    const auto timeout_ticks = ctx->options[SCE_RUDP_OPTION_CONNECTION_TIMEOUT];

    const uint64_t start_tick = rudp_get_ticks(emuenv);

    while (ctx->status.state != SCE_RUDP_STATE_ESTABLISHED) {
        if ((rudp_get_ticks(emuenv) - start_tick) > timeout_ticks) {
            ctx->status.state = SCE_RUDP_STATE_IDLE;
            return rudp_set_last_error(ctx.get(), SCE_RUDP_ERROR_CONN_TIMEOUT);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpBind, int ctx_id, int soc, uint16_t vport, uint8_t mux_mode) {
    TRACY_FUNC(sceRudpBind, ctx_id, soc, vport, mux_mode);
    LOG_INFO("sceRudpBind(ctx_id={}, soc={}, vport={}, mux_mode={})", ctx_id, soc, ntohs(vport), mux_mode);
    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (ctx_id < 0 || soc < 0)
        return SCE_RUDP_ERROR_INVALID_ARGUMENT;

    if (vport == 0)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_VPORT);

    const auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
    if (!ctx)
        return SCE_RUDP_ERROR_INVALID_CONTEXT_ID;

    const auto socket = lock_and_find(soc, emuenv.net.socks, rudp.mutex);
    if (!socket)
        return SCE_RUDP_ERROR_INVALID_CONTEXT_ID;

    if ((socket->bound_ctx_id != 0) && (socket->bound_ctx_id != ctx_id)) {
        LOG_ERROR("Socket already bound to another context: {}", socket->bound_ctx_id);
        return SCE_RUDP_ERROR_ALREADY_BOUND;
    }

    socket->bound_ctx_id = ctx_id;

    ctx->bound = true;
    ctx->bound_socket_id = soc;
    ctx->vport = vport;
    ctx->mux_mode = mux_mode;

    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpCreateContext, SceRudpContextEventHandler handler, void *arg, int *ctxId) {
    TRACY_FUNC(sceRudpCreateContext, handler, arg, ctxId);
    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (!ctxId)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    const auto id = ++rudp.next_id;
    if (id < 0)
        return RET_ERROR(SCE_RUDP_ERROR_MEMORY);

    try {
        RudpContext ctx{
            .ctx_id = id,
            .handler = handler,
            .arg = arg,
        };
        rudp.contexts.emplace(id, std::make_shared<RudpContext>(ctx));

    } catch (const std::bad_alloc &) {
        return SCE_RUDP_ERROR_MEMORY;
    }

    *ctxId = id;

    rudp.io_cv.notify_all();

    LOG_DEBUG_IF(log_rudp, "SceRudpCreateContext: ctxId={}", id);
    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpEnableInternalIOThread, uint32_t stackSize, uint32_t priority) {
    TRACY_FUNC(sceRudpEnableInternalIOThread, stackSize, priority);
    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (rudp.thread_started)
        return RET_ERROR(SCE_RUDP_ERROR_THREAD_IN_USE);

    if (stackSize < 4096)
        stackSize = 4096;

    LOG_DEBUG_IF(log_rudp, "[sceRudpEnableInternalIOThread] stackSize={}, priority={}", stackSize, priority);

    rudp.thread_started = true;
    rudp.io_write_thread = std::thread(rudp_io_write_thread, std::ref(emuenv));
    rudp.io_read_thread = std::thread(rudp_io_read_thread, std::ref(emuenv));

    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpEnableInternalIOThread2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRudpEnd) {
    TRACY_FUNC(sceRudpEnd);
    rudp.inited = false;

    if (rudp.thread_started) {
        if (rudp.io_write_thread.joinable())
            rudp.io_write_thread.join();
        if (rudp.io_read_thread.joinable())
            rudp.io_read_thread.join();
        rudp.thread_started = false;
    }

    rudp.polls.clear();
    rudp.next_poll_id = 0;
    rudp.contexts.clear();
    rudp.next_id = 0;

    return UNIMPLEMENTED();
}

EXPORT(int, sceRudpFlush) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRudpGetContextStatus, int ctx_id, SceRudpContextStatus *status, size_t status_size) {
    TRACY_FUNC(sceRudpGetContextStatus, ctx_id, status, status_size);
    LOG_INFO("sceRudpGetContextStatus, ctx id: {}", ctx_id);

    if (!status || status_size < sizeof(SceRudpContextStatus))
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
    if (!ctx)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);

    *status = ctx->status;

    LOG_INFO("SceRudpGetContextStatus: ctx_id={}, state={}, lostPackets={}, sentPackets={}, rcvdPackets={}, sentBytes={}, rcvdBytes={}, retransmissions={}, rtt={}",
        ctx_id, (int)status->state, status->lostPackets, status->sentPackets, status->rcvdPackets,
        status->sentBytes, status->rcvdBytes, status->retransmissions, status->rtt);

    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpGetLocalInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRudpGetMaxSegmentSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRudpGetNumberOfPacketsToRead) {
    return UNIMPLEMENTED();
}

static size_t expected_option_len(int option) {
    switch (option) {
    case SCE_RUDP_OPTION_NODELAY:
    case SCE_RUDP_OPTION_DELIVERY_CRITICAL:
    case SCE_RUDP_OPTION_ORDER_CRITICAL:
    case SCE_RUDP_OPTION_NONBLOCK:
    case SCE_RUDP_OPTION_STREAM:
        return sizeof(int);
    default:
        return sizeof(uint32_t);
    }
}

EXPORT(int, sceRudpGetOption, int ctx_id, int option, void *value, uint32_t len) {
    TRACY_FUNC(sceRudpGetOption, ctx_id, option, value, len);
    LOG_DEBUG_IF(log_rudp, "SceRudpGetOption: ctx_id={}, option={}, len={}", ctx_id, rudp_option_to_string(option), len);
    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    const auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
    if (!ctx)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);

    if ((option < SCE_RUDP_OPTION_MAX_PAYLOAD) || (option > SCE_RUDP_OPTION_KEEP_ALIVE_TIMEOUT))
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_OPTION);

    if (len != expected_option_len(option)) {
        LOG_ERROR("Unexpected len={} for option={}", len, option);
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);
    }

    if (!value) {
        LOG_ERROR("Invalide value");
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);
    }

    const auto &opt_val = ctx->options[static_cast<SceRudpOption>(option)];
    LOG_DEBUG_IF(log_rudp, "opt val: {}", opt_val);
    std::memcpy(value, &opt_val, len);

    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpGetRemoteInfo) {
    return UNIMPLEMENTED();
}

EXPORT(SceSSize, sceRudpGetSizeReadable, int ctx_id) {
    TRACY_FUNC(sceRudpGetSizeReadable, ctx_id);
    LOG_INFO("sceRudpGetSizeReadable(ctx_id={})", ctx_id);

    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (ctx_id < 0)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
    if (!ctx)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);

    if ((ctx->status.state != SCE_RUDP_STATE_ESTABLISHED) && (ctx->status.state != SCE_RUDP_STATE_CLOSE_WAIT))
        return RET_ERROR(SCE_RUDP_ERROR_NOT_ACCEPTABLE);

    const SceSSize readable_bytes = !ctx->recv_buffer.empty() && (ctx->current_recv_bytes > 0) ? ctx->recv_buffer.front().payload.size() : 0;
    LOG_DEBUG_IF(log_rudp, "sceRudpGetSizeReadable: ctx_id={}, readable_bytes={}, current_recv_bytes={}", ctx_id, readable_bytes, ctx->current_recv_bytes);
    if ((ctx->status.state == SCE_RUDP_STATE_CLOSE_WAIT) && (readable_bytes == 0))
        return RET_ERROR(SCE_RUDP_ERROR_END_OF_DATA);

    return readable_bytes;
}

EXPORT(SceSSize, sceRudpGetSizeWritable, int ctx_id) {
    TRACY_FUNC(sceRudpGetSizeWritable, ctx_id);
    LOG_INFO("sceRudpGetSizeWritable(ctx_id={})", ctx_id);

    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (ctx_id < 0)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
    if (!ctx)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);

    if ((ctx->status.state != SCE_RUDP_STATE_ESTABLISHED) && (ctx->status.state != SCE_RUDP_STATE_CLOSE_WAIT))
        return RET_ERROR(SCE_RUDP_ERROR_NOT_ACCEPTABLE);

    SceSSize available = ctx->options[SCE_RUDP_OPTION_SNDBUF] - ctx->current_send_bytes;
    if (available < 0)
        available = 0;

    LOG_DEBUG_IF(log_rudp, "sceRudpGetSizeWritable(available={})", available);
    return available;
}

EXPORT(int, sceRudpGetStatus, SceRudpStatus *status, uint32_t status_size) {
    TRACY_FUNC(sceRudpGetStatus, status, status_size);
    LOG_INFO("sceRudpGetStatus(status_size={})", status_size);
    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (!status || status_size < sizeof(SceRudpStatus))
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    *status = rudp.status;

    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpInit, void *pool, uint32_t poolSize) {
    TRACY_FUNC(sceRudpInit, pool, poolSize);
    LOG_DEBUG_IF(log_rudp, "SceRudpInit: pool={}, poolSize={}", pool, poolSize);

    if (!pool || poolSize == 0)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    if (rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_ALREADY_INITIALIZED);

    rudp.pool = pool;
    rudp.pool_size = poolSize;
    rudp.inited = true;

    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpInitiate, int ctx_id, const struct SceNetSockaddr *to, SceNetSocklen_t to_len, uint16_t vport) {
    TRACY_FUNC(sceRudpInitiate, ctx_id, to, to_len, vport);
    LOG_INFO("sceRudpInitiate, ctx id: {}, to len: {}, vport: {}", ctx_id, to_len, vport);

    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (ctx_id < 0 || !to || to_len == 0)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    const auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
    if (!ctx)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);

    if (ctx->status.state != SCE_RUDP_STATE_IDLE)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_ACCEPTABLE);

    if (!ctx->bound)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_BOUND);

    // Check for duplicate peer address (ADDR_IN_USE, if same socket/peer already used)
    for (const auto &[id, other_ctx] : rudp.contexts) {
        if (other_ctx.get() != ctx.get() && other_ctx->bound_socket_id == ctx->bound_socket_id && other_ctx->peer_addr_len == to_len && memcmp(&other_ctx->peer_addr, to, to_len) == 0) {
            return RET_ERROR(SCE_RUDP_ERROR_ADDR_IN_USE);
        }
    }

    // Save peer info
    memcpy(&ctx->peer_addr, to, to_len);
    ctx->peer_addr_len = to_len;
    ctx->vport = 0; // vport unused in this implementation

    // Options
    const auto nonblock = ctx->options[SCE_RUDP_OPTION_NONBLOCK];
    const auto timeout_ms = ctx->options[SCE_RUDP_OPTION_CONNECTION_TIMEOUT];

    const auto socket = lock_and_find(ctx->bound_socket_id, emuenv.net.socks, rudp.mutex);
    if (!socket)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_SOCKET);

    LOG_DEBUG_IF(log_rudp, "SceRudpInitiate: sid: {}, vport addr={}", ctx->bound_socket_id,
        ntohs(((SceNetSockaddrIn *)&ctx->peer_addr)->sin_vport));

    // Send SYN segment (manually emulate RUDP behavior)
    RudpSegment syn_seg{
        .header = {
            .type = RUDP_SEGMENT_TYPE_SYN,
            .sequence_number = 0, // Initial sequence number
            .timestamp = rudp_get_ticks(emuenv), // Current timestamp
        },
    };

    LOG_DEBUG_IF(log_rudp, "SceRudpInitiate: Sending SYN segment (type={}, seq={}, timestamp={})",
        rudp_segment_type_to_string(syn_seg.header.type),
        syn_seg.header.sequence_number,
        syn_seg.header.timestamp);

    // Enqueue SYN to be sent
    auto syn_buffer = convert_segment_to_buffer(syn_seg);
    flush_buffer(ctx.get(), syn_buffer);
    rudp.status.sentSynPackets++;

    // Set state and start time
    ctx->status.state = SCE_RUDP_STATE_SYN_SENT;
    ctx->options[SCE_RUDP_OPTION_LAST_ERROR] = SCE_RUDP_SUCCESS;
    ctx->timestamp_start_connect = rudp_get_ticks(emuenv);
    ctx->timestamp_syn_sent = ctx->timestamp_start_connect;

    if (nonblock)
        return SCE_RUDP_SUCCESS;

    const uint64_t start_tick_ms = rudp_get_ticks(emuenv);
    uint64_t last_send_tick = start_tick_ms;
    int resend_count = 0;

    // Retransmission intervals: 1s, 2s, 4s, 8s, 16s, then 16s continuously
    constexpr std::array<uint32_t, 5> resend_intervals_ms = { 1000, 2000, 4000, 8000, 16000 };

    while (ctx->status.state != SCE_RUDP_STATE_ESTABLISHED) {
        const uint64_t now = rudp_get_ticks(emuenv);

        if ((now - start_tick_ms) > timeout_ms) {
            std::lock_guard lock(rudp.mutex);
            ctx->options[SCE_RUDP_OPTION_LAST_ERROR] = SCE_RUDP_ERROR_CONN_TIMEOUT;
            ctx->status.state = SCE_RUDP_STATE_IDLE;
            return RET_ERROR(SCE_RUDP_ERROR_CONN_TIMEOUT);
        }

        uint32_t resend_interval = (resend_count < resend_intervals_ms.size())
            ? resend_intervals_ms[resend_count]
            : resend_intervals_ms.back();

        // Check if it's time to resend SYN
        if (((now - last_send_tick) >= resend_interval) && (ctx->status.state == SCE_RUDP_STATE_SYN_SENT)) {
            syn_seg.header.sequence_number = ++ctx->last_send_seq;
            syn_seg.header.timestamp = now; // Update timestamp for retransmission

            LOG_DEBUG_IF(log_rudp, "SceRudpInitiate: Resending SYN (count={}, seq={}, ts={})",
                resend_count + 1,
                syn_seg.header.sequence_number,
                syn_seg.header.timestamp);

            auto syn_buffer = convert_segment_to_buffer(syn_seg);
            flush_buffer(ctx.get(), syn_buffer);
            ctx->timestamp_syn_sent = now;
            rudp.status.sentSynPackets++;

            ++resend_count;
            last_send_tick = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpNetReceived) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRudpPollCancel) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRudpPollControl, int pollId, int op, int ctxId, uint16_t events) {
    TRACY_FUNC(sceRudpPollControl, pollId, op, ctxId, events);
    LOG_INFO("sceRudpPollControl: pollId={}, op={}, ctxId={}, events={}", pollId, rudp_poll_op_to_string(op), ctxId, rudp_poll_event_flags_to_string(events));

    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (pollId < 0 || ctxId < 0 || (op != SCE_RUDP_POLL_OP_ADD && op != SCE_RUDP_POLL_OP_MODIFY && op != SCE_RUDP_POLL_OP_REMOVE))
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    auto poll = lock_and_find(pollId, rudp.polls, rudp.mutex);
    if (!poll || !poll->max_size)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_POLL_ID);

    auto ctx = lock_and_find(ctxId, rudp.contexts, rudp.mutex);
    if (!ctx)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);

    switch (op) {
    case SCE_RUDP_POLL_OP_ADD: {
        if (poll->entries.size() >= poll->max_size)
            return RET_ERROR(SCE_RUDP_ERROR_TOO_MANY_CONTEXTS);
        {
            std::lock_guard lock(rudp.poll_mutex);
            if (!poll->entries.try_emplace(ctxId, events).second)
                return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);

            ctx->is_handled_by_poll = true;
        }
        break;
    }
    case SCE_RUDP_POLL_OP_MODIFY: {
        if (!poll->entries.contains(ctxId))
            return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
        {
            std::lock_guard lock(rudp.poll_mutex);
            poll->entries[ctxId] = events;
        }
        break;
    }
    case SCE_RUDP_POLL_OP_REMOVE: {
        if (!poll->entries.contains(ctxId))
            return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);

        {
            std::lock_guard lock(rudp.poll_mutex);
            if (ctx->is_handled_by_poll) {
                ctx->is_readable = false; // Reset readable state
                ctx->is_handled_by_poll = false;
            }
            poll->entries.erase(ctxId);
        }
        break;
    }
    default:
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);
    }

    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpPollCreate, uint32_t size) {
    TRACY_FUNC(sceRudpPollCreate, size);
    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (size == 0)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    int poll_id = ++rudp.next_poll_id;

    auto poll = std::make_shared<RudpPoll>();
    if (!poll)
        return RET_ERROR(SCE_RUDP_ERROR_MEMORY);

    poll->max_size = size;

    {
        std::lock_guard<std::mutex> lock(rudp.mutex);
        rudp.polls.emplace(poll_id, poll);
    }

    LOG_INFO("sceRudpPollCreate: id={}, size={}", poll_id, size);
    return poll_id;
}

EXPORT(int, sceRudpPollDestroy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRudpPollWait, int poll_id, SceRudpPollEvent *events, uint32_t event_len, SceRudpUsec timeout) {
    TRACY_FUNC(sceRudpPollWait, poll_id, events, event_len, timeout);
    LOG_INFO("sceRudpPollWait: poll_id={}, event_len={}, timeout={}", poll_id, event_len, timeout);

    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (poll_id < 0 || !events || event_len == 0)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    auto poll = lock_and_find(poll_id, rudp.polls, rudp.mutex);
    if (!poll)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_POLL_ID);

    if (poll->entries.empty()) {
        LOG_WARN("Entries is empty");
        return 0;
    }

    std::vector<RudpPollSelect> poll_sockets;
    for (const auto &[ctx_id, entry] : poll->entries) {
        auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
        if (!ctx)
            continue;

        auto socket = lock_and_find(ctx->bound_socket_id, emuenv.net.socks, rudp.mutex);
        if (!socket) {
            LOG_ERROR("sceRudpPollWait: Invalid socket for ctx_id={}, socket id={}", ctx_id, ctx->bound_socket_id);
            continue;
        }

        auto posixSocket = std::dynamic_pointer_cast<PosixSocket>(socket)->sock;
        if ((posixSocket == INVALID_SOCKET)) {
            LOG_ERROR("sceRudpPollWait: Invalid posix socket for ctx_id={}, socket id={}", ctx_id, ctx->bound_socket_id);
            continue;
        }

        poll_sockets.emplace_back(ctx_id, entry, posixSocket);
    }

    if (poll_sockets.empty())
        return 0; // No valid sockets to poll

    std::map<uint32_t, uint16_t> events_type;
    const auto ret = rudp_poll_select(poll_sockets, timeout, events_type);
    if (ret <= 0) {
        return ret; // Error from select
    }

    int num_events = 0;
    for (const auto &[ctx_id, enty] : poll->entries) {
        auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
        if (!ctx)
            continue;

        if (events_type[ctx_id] && num_events < static_cast<int>(event_len)) {
            events[num_events++] = {
                .ctxId = ctx_id,
                .reqEvents = enty,
                .rtnEvents = events_type[ctx_id],
            };
        }
    }

    LOG_DEBUG_IF(log_rudp, "num event:{}, ret: {}", num_events, ret);
    return num_events;
}

EXPORT(int, sceRudpProcessEvents) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceRudpRead, int ctx_id, void *data, uint32_t len, uint8_t flags, SceRudpReadInfo *info) {
    TRACY_FUNC(sceRudpRead, ctx_id, data, len, flags, info);
    LOG_INFO("sceRudpRead(ctx_id={}, data={}, len={}, flags={:#x})", ctx_id, (void *)data, len, flags);
    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (ctx_id < 0 || data == nullptr || len == 0)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    const auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
    if (!ctx)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);

    if ((ctx->status.state != SCE_RUDP_STATE_ESTABLISHED) && (ctx->status.state != SCE_RUDP_STATE_CLOSE_WAIT))
        return RET_ERROR(SCE_RUDP_ERROR_NOT_ACCEPTABLE);

    {
        std::unique_lock lock(rudp.recv_mutex);
        if (ctx->recv_buffer.empty()) {
            if (flags & SCE_RUDP_MSG_DONTWAIT)
                return RET_ERROR(SCE_RUDP_ERROR_WOULDBLOCK);

            LOG_WARN("sceRudpRead: waiting for data");
            rudp.recv_cv.wait(lock, [&] { return !ctx->recv_buffer.empty(); });
        }
    }

    auto &front = ctx->recv_buffer.front();
    const auto payload_len = front.payload.size();
    LOG_DEBUG_IF(log_rudp, "sceRudpRead, packet available={}, bytes avaiable={}, payload size={}", ctx->recv_buffer.size(), ctx->current_recv_bytes, payload_len);
    memcpy(data, front.payload.data(), payload_len);

    if (info) {
        info->size = sizeof(SceRudpReadInfo);
        // info->retransmissionCount = front.retransmission_count;
        // info->retransmissionDelay = front.retransmission_delay;
        // info->retransmissionDelay2 = front.retransmission_delay2;
        // info->flags = front.header.flags;

        info->sequenceNumber = front.header.sequence_number;
        info->timestamp = front.header.timestamp;
        LOG_DEBUG_IF(log_rudp, "sceRudpRead, info: size={}, retransmissionCount={}, retransmissionDelay={}, retransmissionDelay2={}, flags={}, sequenceNumber={}, timestamp={}",
            info->size, info->retransmissionCount, info->retransmissionDelay, info->retransmissionDelay2,
            info->flags, info->sequenceNumber, info->timestamp);
    }

    ctx->recv_buffer.pop();

    ctx->current_recv_bytes -= payload_len;

    return payload_len;
}

EXPORT(int, sceRudpSetEventHandler, SceRudpEventHandler handler, void *arg) {
    TRACY_FUNC(sceRudpSetEventHandler, handler, arg);

    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (handler == nullptr)
        return RET_ERROR(SCE_RUDP_ERROR_NO_EVENT_HANDLER);

    rudp.event_handler = handler;
    rudp.event_handler_arg = arg;

    LOG_DEBUG_IF(log_rudp, "SceRudpSetEventHandler");

    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpSetMaxSegmentSize, uint16_t mss) {
    TRACY_FUNC(sceRudpSetMaxSegmentSize, mss);
    LOG_INFO("SceRudpSetMaxSegmentSize: mss={}", mss);
    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (!rudp.contexts.empty())
        return RET_ERROR(SCE_RUDP_ERROR_NOT_ACCEPTABLE);

    rudp.max_segment_size = mss;
    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpSetOption, int ctx_id, int option, const void *value, uint32_t len) {
    TRACY_FUNC(sceRudpSetOption, ctx_id, option, value, len);
    LOG_DEBUG_IF(log_rudp, "SceRudpSetOption: ctx_id={}, option={}, value={}, len={}", ctx_id, rudp_option_to_string(option),
        *reinterpret_cast<const uint32_t *>(value), len);
    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    const auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
    if (!ctx)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);

    if ((option < SCE_RUDP_OPTION_MAX_PAYLOAD) || (option > SCE_RUDP_OPTION_KEEP_ALIVE_TIMEOUT))
        return rudp_set_last_error(ctx.get(), SCE_RUDP_ERROR_INVALID_OPTION);

    if (len != expected_option_len(option)) {
        LOG_ERROR("Unexpected len={} for option={}", len, option);
        return rudp_set_last_error(ctx.get(), SCE_RUDP_ERROR_INVALID_ARGUMENT);
    }

    if (!value) {
        LOG_ERROR("Invalide value");
        return rudp_set_last_error(ctx.get(), SCE_RUDP_ERROR_INVALID_ARGUMENT);
    }

    ctx->options[static_cast<SceRudpOption>(option)] = *reinterpret_cast<const int32_t *>(value);
    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpTerminate, int ctx_id) {
    TRACY_FUNC(sceRudpTerminate, ctx_id);
    LOG_DEBUG_IF(log_rudp, "sceRudpTerminate(ctx_id={})", ctx_id);
    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (ctx_id <= 0)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
    if (!ctx)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);

    {
        std::lock_guard lock(rudp.mutex);
        ctx->recv_buffer = {};
        ctx->send_buffer = {};

        rudp.recv_cv.notify_all();
        rudp.contexts.erase(ctx_id);
    }

    {
        std::lock_guard lock(rudp.poll_mutex);
        for (auto &[_, poll] : rudp.polls) {
            if (poll->entries.contains(ctx_id))
                poll->entries.erase(ctx_id);
        }
    }

    return SCE_RUDP_SUCCESS;
}

EXPORT(int, sceRudpWrite, int ctx_id, const void *data, uint32_t len, uint8_t flags) {
    TRACY_FUNC(sceRudpWrite, ctx_id, data, len, flags);
    LOG_INFO("sceRudpWrite(ctx_id={}, data={}, len={}, flags={:#x})", ctx_id, (void *)data, len, flags);

    if (!rudp.inited)
        return RET_ERROR(SCE_RUDP_ERROR_NOT_INITIALIZED);

    if (ctx_id < 0 || data == nullptr || len == 0)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_ARGUMENT);

    const auto ctx = lock_and_find(ctx_id, rudp.contexts, rudp.mutex);
    if (!ctx)
        return RET_ERROR(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);

    if (len > ctx->options[SCE_RUDP_OPTION_SNDBUF]) {
        LOG_ERROR("sceRudpWrite: len ({}) exceeds maximum allowed size ({}).", len, ctx->options[SCE_RUDP_OPTION_SNDBUF]);
        return rudp_set_last_error(ctx.get(), SCE_RUDP_ERROR_MSG_TOO_LARGE);
    }

    if (ctx->status.state != SCE_RUDP_STATE_ESTABLISHED)
        return rudp_set_last_error(ctx.get(), SCE_RUDP_ERROR_NOT_ACCEPTABLE);
    {
        std::unique_lock lock(rudp.send_mutex);
        if ((ctx->current_send_bytes + len) >= ctx->options[SCE_RUDP_OPTION_SNDBUF]) {
            if (flags & SCE_RUDP_MSG_DONTWAIT)
                return rudp_set_last_error(ctx.get(), SCE_RUDP_ERROR_WOULDBLOCK);

            LOG_WARN("sceRudpWrite: waiting for space in send buffer");
            rudp.send_cv.wait(lock, [&] {
                return ((ctx->current_send_bytes + len) <= ctx->options[SCE_RUDP_OPTION_SNDBUF]);
            });
        }
    }

    if ((len + sizeof(RudpSegmentHeader)) > ctx->max_segment_size) {
        if (!ctx->aggregation_buffer.empty()) {
            LOG_WARN("sceRudpWrite: aggregation buffer is not empty, flushing it before writing new data");
            flush_buffer(ctx.get(), ctx->aggregation_buffer);
        }

        const uint32_t max_payload_per_segment = ctx->max_segment_size - sizeof(RudpSegmentHeader);

        // Calculate the number of segments needed, rounding up
        uint8_t segment_count = (len + max_payload_per_segment - 1) / max_payload_per_segment;
        uint16_t sequence_base = ++ctx->last_send_seq; // Same sequence number for all segments in this write

        for (uint8_t i = 0; i < segment_count; i++) {
            const uint32_t offset = i * max_payload_per_segment;
            const uint16_t segment_len = std::min(max_payload_per_segment, len - offset);

            RudpSegment segment;
            segment.header = {
                .type = RUDP_SEGMENT_TYPE_DATA,
                .sequence_number = sequence_base,
                .index = i,
                .count = segment_count,
                .timestamp = rudp_get_ticks(emuenv),
                .payload_len = segment_len,
            };

            segment.payload.resize(segment_len);
            std::memcpy(segment.payload.data(), (uint8_t *)data + offset, segment_len);
            LOG_DEBUG_IF(log_rudp, "sceRudpWrite: segment {} of {}: seq={}, index={}, count={}, len={}, offset={}",
                i + 1, segment_count, segment.header.sequence_number, segment.header.index, segment.header.count, segment_len, offset);

            auto segment_buffer = convert_segment_to_buffer(std::move(segment));
            flush_buffer(ctx.get(), segment_buffer); // Flush each segment immediately
        }
    } else {
        RudpSegment segment = {
            .header = {
                .type = RUDP_SEGMENT_TYPE_DATA,
                .sequence_number = ++ctx->last_send_seq,
                .timestamp = rudp_get_ticks(emuenv),
                .payload_len = len, // Set the payload length
            },
        };
        segment.payload.resize(len);
        std::memcpy(segment.payload.data(), data, len);

        auto segment_buffer = convert_segment_to_buffer(std::move(segment));

        if (flags & SCE_RUDP_MSG_LATENCY_CRITICAL) {
            flush_buffer(ctx.get(), ctx->aggregation_buffer); // Flush aggregation buffer if latency critical
            flush_buffer(ctx.get(), segment_buffer); // Flush the segment immediately
        } else {
            if (ctx->aggregation_buffer.empty())
                ctx->last_aggregation_time = rudp_get_ticks(emuenv);
            else if ((ctx->aggregation_buffer.size() + sizeof(RudpSegmentHeader) + len) > ctx->max_segment_size)
                flush_buffer(ctx.get(), ctx->aggregation_buffer); // Flush if adding this segment would exceed max size

            ctx->aggregation_buffer.insert(ctx->aggregation_buffer.end(), segment_buffer.begin(), segment_buffer.end());
        }
    }
    ctx->current_send_bytes += len;

    rudp.send_cv.notify_all();

    LOG_DEBUG_IF(log_rudp, "sceRudpWrite: buffered {} bytes, total in buffer: {}", len, ctx->current_send_bytes);

    return static_cast<int>(len);
}
