#include <memory>

// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#ifdef USE_GDBSTUB

#include <host/state.h>
#include <util/log.h>

#include <gdbstub/functions.h>

#include <cpu/functions.h>
#include <kernel/functions.h>

#include <spdlog/fmt/bundled/printf.h>
#include <sstream>

// Sockets
#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define LOG_GDB_LEVEL 1

#if LOG_GDB_LEVEL >= 1
#define LOG_GDB LOG_INFO
#else
#define LOG_GDB(a, ...)
#endif

#if LOG_GDB_LEVEL >= 2
#define LOG_GDB_DEBUG LOG_INFO
#else
#define LOG_GDB_DEBUG(a, ...)
#endif

// Credit to jfhs for their GDB stub for RPCS3 which this stub is based on.

typedef char PacketData[1200];

struct PacketCommand {
    char *data{};
    int64_t length = -1;

    int64_t begin_index = -1;
    int64_t end_index = -1;
    int64_t content_length = -1;

    char *content_start{};
    char *checksum_start{};

    uint8_t checksum = 0;

    bool is_valid = false;
};

typedef std::function<std::string(HostState &state, PacketCommand &command)> PacketFunction;

struct PacketFunctionBundle {
    std::string name;
    PacketFunction function;
};

std::string content_string(PacketCommand &command) {
    return std::string(command.content_start, static_cast<unsigned long>(command.content_length));
}

// Hexes
std::string be_hex(uint32_t value) {
    return fmt::format("{:0>8x}", htonl(value));
}

std::string to_hex(uint32_t value) {
    return fmt::format("{:0>8x}", value);
}

uint32_t parse_hex(const std::string &hex) {
    std::stringstream stream;
    uint32_t value;
    stream << std::hex << hex;
    stream >> value;
    return value;
}

uint8_t make_checksum(const char *data, int64_t length) {
    size_t sum = 0;

    for (int64_t a = 0; a < length; a++) {
        sum += data[a];
    }

    return static_cast<uint8_t>(sum % 256);
}

PacketCommand parse_command(char *data, int64_t length) {
    PacketCommand command = {};

    command.data = data;
    command.length = length;

    // TODO: Use std::find() to find packet begin and end.
    if (length > 1)
        command.begin_index = 1;
    for (int64_t a = 0; a < length; a++) {
        if (data[a] == '#')
            command.end_index = a;
    }

    command.is_valid = command.begin_index != -1 && command.end_index != -1
        && command.end_index > command.begin_index && command.end_index + 2 < length;
    if (!command.is_valid)
        return command;

    command.content_start = command.data + command.begin_index;
    command.content_length = command.end_index - command.begin_index;

    command.checksum_start = command.data + command.end_index + 1;
    command.checksum = (uint8_t)std::stoul(
        std::string(command.checksum_start, 2), nullptr, 16);

    command.is_valid = make_checksum(command.content_start, command.content_length) == command.checksum;
    if (!command.is_valid)
        return command;

    return command;
}

int64_t server_reply(GDBState &state, const char *data, int64_t length) {
    uint8_t checksum = make_checksum(data, length);
    std::string packet_data = fmt::format("${}#{:0>2x}", std::string(data, length), checksum);

    return send(state.client_socket, &packet_data[0], packet_data.size(), 0);
}

int64_t server_reply(GDBState &state, const char *text) {
    return server_reply(state, text, strlen(text));
}

int64_t server_ack(GDBState &state, char ack = '+') {
    return send(state.client_socket, &ack, 1, 0);
}

std::string cmd_supported(HostState &state, PacketCommand &command) {
    return "multiprocess-;swbreak+;hwbreak-;qRelocInsn-;fork-events-;vfork-events-;"
           "exec-events-;vContSupported+;QThreadEvents-;no-resumed-;xmlRegisters=arm";
}

std::string cmd_reply_empty(HostState &state, PacketCommand &command) {
    return "";
}

SceUID select_thread(HostState &state, int thread_id) {
    if (thread_id == 0) {
        if (state.kernel.threads.empty())
            return -1;
        return state.kernel.threads.begin()->first;
    }
    return thread_id;
}

std::string cmd_set_current_thread(HostState &state, PacketCommand &command) {
    int32_t thread_id = std::stoi(
        std::string(command.content_start + 2, static_cast<unsigned long>(command.content_length - 2)));

    switch (command.content_start[1]) {
    case 'c':
        LOG_GDB("GDB Server Deprecated Continue Option 'c'");
        // state.gdb.current_continue_thread = select_thread(state, thread_id);
        break;
    case 'g':
        state.gdb.current_thread = select_thread(state, thread_id);
        break;
    default:
        LOG_GDB("GDB Server Unknown Set Current Thread OP. {}", command.content_start[1]);
        break;
    }

    return "OK";
}

std::string cmd_get_current_thread(HostState &state, PacketCommand &command) {
    return "QC" + to_hex(static_cast<uint32_t>(state.gdb.current_thread));
}

uint32_t fetch_reg(CPUState &state, uint32_t reg) {
    if (reg <= 12) {
        return read_reg(state, reg);
    }

    if (reg == 13)
        return read_sp(state);
    if (reg == 14)
        return read_lr(state);
    if (reg == 15)
        return read_pc(state);

    if (reg <= 23) {
        float value = read_float_reg(state, reg - 16);
        return *reinterpret_cast<uint32_t *>(&value);
    }

    if (reg == 24)
        return read_fpscr(state);
    if (reg == 25)
        return read_cpsr(state);

    LOG_GDB("GDB Server Queried Invalid Register {}", reg);
    return 0;
}

std::string cmd_read_registers(HostState &state, PacketCommand &command) {
    if (state.gdb.current_thread == -1)
        return "";
    CPUState &cpu = *state.kernel.threads[state.gdb.current_thread]->cpu.get();

    std::stringstream stream;
    for (uint32_t a = 0; a <= 15; a++) {
        stream << be_hex(fetch_reg(cpu, a));
    }

    return stream.str();
}

std::string cmd_write_registers(HostState &state, PacketCommand &command) { return ""; }

std::string cmd_read_register(HostState &state, PacketCommand &command) {
    if (state.gdb.current_thread == -1)
        return "";
    CPUState &cpu = *state.kernel.threads[state.gdb.current_thread]->cpu.get();

    std::string content = content_string(command);
    int32_t reg = parse_hex(content.substr(1, content.size() - 1));

    return be_hex(fetch_reg(cpu, static_cast<uint32_t>(reg)));
}

std::string cmd_write_register(HostState &state, PacketCommand &command) {
    return "";
}

std::string cmd_read_memory(HostState &state, PacketCommand &command) {
    std::string content = content_string(command);
    size_t pos = content.find(',');

    std::string first = content.substr(1, pos - 1);
    std::string second = content.substr(pos + 1, content.size() - pos);
    uint32_t address = parse_hex(first);
    uint32_t length = parse_hex(second);

    auto page_first = static_cast<uint32_t>(address / KB(4));
    auto page_length = static_cast<uint32_t>(length + KB(4)) / KB(4);

    auto pages_begin = state.mem.allocated_pages.begin();
    auto range_end = pages_begin + page_first + page_length;

    bool memory_none = std::find(pages_begin + page_first, range_end, 0) != range_end;
    bool memory_null = std::find(pages_begin + page_first, range_end, 1) != range_end;
    bool memory_safe = !(memory_none || memory_null);

    if (!memory_safe) {
        LOG_GDB("Reading from unsafe page. {} - {}", page_first, page_length);
        return "EAA";
    }

    std::stringstream stream;

    for (int a = 0; a < length; a++) {
        stream << fmt::format("{:0>2x}", static_cast<uint8_t>(state.mem.memory[address + a]));
    }

    return stream.str();
}

std::string cmd_write_memory(HostState &state, PacketCommand &command) { return ""; }

std::string cmd_detach(HostState &state, PacketCommand &command) { return "OK"; }

std::string cmd_continue(HostState &state, PacketCommand &command) {
    std::string content = content_string(command);

    uint64_t index = 5;
    uint64_t next = 0;
    do {
        next = content.find(';', index + 1);
        std::string text = content.substr(index + 1, next - index - 1);

        uint64_t colon = text.find(':');

        char cmd = text[0];
        switch (cmd) {
        case 'c':
        case 'C':
        case 's':
        case 'S': {
            bool step = cmd == 's' || cmd == 'S';
            if (colon != std::string::npos) {
                int32_t point = std::stoi(text.substr(colon + 1));
                ThreadStatePtr thread = state.kernel.threads[point];
                if (thread) {
                    thread->to_do = step ? ThreadToDo::step : ThreadToDo::run;
                    thread->something_to_do.notify_one();
                }
            } else {
                for (const auto &thread : state.kernel.threads) {
                    if (thread.second) {
                        thread.second->to_do = step ? ThreadToDo::step : ThreadToDo::run;
                        thread.second->something_to_do.notify_one();
                    }
                }
            }

            if (!step) {
                bool hit_break = false;
                while (!hit_break) {
                    for (const auto &thread : state.kernel.threads) {
                        if (thread.second->to_do == ThreadToDo::wait && hit_breakpoint(*thread.second->cpu)) {
                            hit_break = true;
                            break;
                        }
                    }
                }
            }

            return "S05";
        }
        default:
            LOG_GDB("Unsupported vCont command '{}'", cmd);
            break;
        }

        index = next;
    } while (next != std::string::npos);

    return "";
}

std::string cmd_continue_supported(HostState &state, PacketCommand &command) {
    return "vCont;c;C;s;S;t;r";
}

std::string cmd_kill(HostState &state, PacketCommand &command) {
    return "OK";
}

std::string cmd_die(HostState &state, PacketCommand &command) {
    state.gdb.server_die = true;
    return "";
}

std::string cmd_attached(HostState &state, PacketCommand &command) { return "1"; }

std::string cmd_thread_status(HostState &state, PacketCommand &command) { return "T0"; }

std::string cmd_reason(HostState &state, PacketCommand &command) { return "S05"; }

// TODO: Implement qsThreadInfo if list becomes too large.
std::string cmd_list_threads(HostState &state, PacketCommand &command) {
    std::stringstream stream;

    stream << "m";

    uint32_t count = 0;
    for (const auto &thread : state.kernel.threads) {
        stream << to_hex(static_cast<uint32_t>(thread.first));
        if (count != state.kernel.threads.size() - 1)
            stream << ",";
        count++;
    }

    stream << "l";

    return stream.str();
}

std::string cmd_add_breakpoint(HostState &state, PacketCommand &command) {
    std::string content = content_string(command);
    uint32_t type, address, kind;

    uint64_t first = content.find(',');
    uint64_t second = content.find(',', first + 1);
    type = static_cast<uint32_t>(std::stol(content.substr(1, first - 1)));
    address = parse_hex(content.substr(first + 1, second - 1 - first));
    kind = static_cast<uint32_t>(std::stol(content.substr(second + 1, content.size() - second - 1)));

    LOG_GDB("GDB Server New Breakpoint at {} ({}, {}).", log_hex(address), type, kind);
    add_breakpoint(state.mem, address);

    return "OK";
}

std::string cmd_remove_breakpoint(HostState &state, PacketCommand &command) {
    std::string content = content_string(command);
    uint32_t type, address, kind;

    uint64_t first = content.find(',');
    uint64_t second = content.find(',', first + 1);
    type = static_cast<uint32_t>(std::stol(content.substr(1, first - 1)));
    address = parse_hex(content.substr(first + 1, second - 1 - first));
    kind = static_cast<uint32_t>(std::stol(content.substr(second + 1, content.size() - second - 1)));

    LOG_GDB("GDB Server Removed Breakpoint at {} ({}, {}).", log_hex(address), type, kind);
    remove_breakpoint(state.mem, address);

    return "OK";
}

std::string cmd_deprecated(HostState &state, PacketCommand &command) {
    LOG_GDB("GDB Server: Deprecated Packet. {}", content_string(command));

    return "";
}

std::string cmd_unimplemented(HostState &state, PacketCommand &command) {
    LOG_GDB("GDB Server: Unimplemented Packet. {}", content_string(command));

    return "";
}

PacketFunctionBundle functions[] = {
    { "!", cmd_unimplemented },
    { "?", cmd_reason },
    { "A", cmd_unimplemented },
    { "b", cmd_unimplemented },
    { "B", cmd_unimplemented },
    { "bc", cmd_unimplemented },
    { "bs", cmd_unimplemented },
    { "c", cmd_deprecated },
    { "C", cmd_deprecated },
    { "d", cmd_unimplemented },
    { "D", cmd_detach },
    { "F", cmd_unimplemented },
    { "g", cmd_read_registers },
    { "G", cmd_write_registers },
    { "H", cmd_set_current_thread },
    { "i", cmd_unimplemented },
    { "I", cmd_unimplemented },
    { "k", cmd_die },
    { "m", cmd_read_memory },
    { "M", cmd_write_memory },
    { "p", cmd_read_register },
    { "P", cmd_write_register },
    { "qfThreadInfo", cmd_list_threads },
    { "qsThreadInfo", cmd_unimplemented },
    { "qSupported", cmd_supported },
    { "qAttached", cmd_attached },
    { "qTStatus", cmd_thread_status },
    { "qC", cmd_get_current_thread },
    { "q", cmd_unimplemented },
    { "Q", cmd_unimplemented },
    { "r", cmd_unimplemented },
    { "R", cmd_unimplemented },
    { "s", cmd_unimplemented },
    { "S", cmd_unimplemented },
    { "t", cmd_unimplemented },
    { "T", cmd_unimplemented },
    { "vAttach", cmd_unimplemented },
    { "vCont?", cmd_continue_supported },
    { "vCont", cmd_continue },
    { "vCtrlC", cmd_unimplemented },
    { "vFile", cmd_unimplemented },
    { "vFlashErase", cmd_unimplemented },
    { "vFlashWrite", cmd_unimplemented },
    { "vFlashDone", cmd_unimplemented },
    { "vKill", cmd_kill },
    { "vMustReplyEmpty", cmd_reply_empty },
    { "vRun", cmd_unimplemented },
    { "vStopped", cmd_unimplemented },
    { "v", cmd_unimplemented },
    { "X", cmd_unimplemented },
    { "z", cmd_remove_breakpoint },
    { "Z", cmd_add_breakpoint },
};

static bool begins(PacketCommand &command, const std::string &small) {
    if (small.size() > command.content_length)
        return false;

    return std::memcmp(command.content_start, small.c_str(), small.size()) == 0;
}

static int64_t server_next(HostState &state) {
    PacketData buffer;

    // Wait for the server to close or a packet to be received.
    fd_set readSet;
    timeval timeout = { 1, 0 };
    do {
        readSet = { 0 };
        FD_SET(state.gdb.client_socket, &readSet);
    } while (select(state.gdb.client_socket + 1, &readSet, nullptr, nullptr, &timeout) < 1 && !state.gdb.server_die);
    if (state.gdb.server_die)
        return -1;

    int64_t length = recv(state.gdb.client_socket, buffer, sizeof(buffer), 0);
    buffer[length] = '\0';

    if (length <= 0) {
        LOG_GDB("GDB Server Connection Closed");
        return -1;
    }

    for (int64_t a = 0; a < length; a++) {
        switch (buffer[a]) {
        case '+': {
            break; // Cool.
        }
        case '-': {
            LOG_GDB("GDB Server Transmission Error. {}", std::string(buffer, length));
            server_reply(state.gdb, state.gdb.last_reply.c_str());
            break;
        }
        case '$': {
            server_ack(state.gdb, '+');

            PacketCommand command = parse_command(buffer + a, length - a);
            if (command.is_valid) {
                std::string debug_func_name = "INVALID FUNC";

                for (const auto &function : functions) {
                    if (begins(command, function.name)) {
                        debug_func_name = function.name;
                        state.gdb.last_reply = function.function(state, command);
                        if (state.gdb.server_die)
                            break;
                        server_reply(state.gdb, state.gdb.last_reply.c_str());
                        break;
                    }
                }

                a += command.content_length + 3;

                LOG_GDB_DEBUG("GDB Server Recognized Command as {}. {}", debug_func_name,
                    std::string(command.content_start, command.content_length));
            } else {
                server_ack(state.gdb, '-');

                LOG_GDB("GDB Server Invalid Command. {}", std::string(buffer, length));
            }
            break;
        }
        default: break;
        }
        if (state.gdb.server_die)
            break;
    }

    return length;
}

static void server_listen(HostState &state) {
    state.gdb.is_running = true;

    state.gdb.client_socket = accept(state.gdb.listen_socket, nullptr, nullptr);

    if (state.gdb.client_socket == -1) {
        LOG_GDB("GDB Server Failed: Could not accept socket.");
        return;
    }

    for (const auto &thread : state.kernel.threads) {
        stop(*thread.second->cpu);
        thread.second->to_do = ThreadToDo::wait;
    }

    LOG_GDB("GDB Server Received Connection");

    int64_t status;

    do {
        status = server_next(state);
    } while (status >= 0 && !state.gdb.server_die);

    state.gdb.is_running = false;

    server_close(state);
}

void server_open(HostState &state) {
    LOG_GDB("Starting GDB Server...");

#ifdef _WIN32
    int32_t err = WSAStartup(MAKEWORD(2, 2), &state.gdb.wsaData);
    if (err) {
        LOG_GDB("GDB Server Failed: Could not start WSA service.");
        return;
    }
#endif

    state.gdb.listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (state.gdb.listen_socket == -1) {
        LOG_GDB("GDB Server Failed: Could not create socket.");
        return;
    }

    sockaddr_in socket_address = { 0 };
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(GDB_SERVER_PORT);
#ifdef _WIN32
    socket_address.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
#else
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
#endif

    if (bind(state.gdb.listen_socket, (sockaddr *)&socket_address, sizeof(socket_address)) == -1) {
        LOG_GDB("GDB Server Failed: Could not bind socket.");
        return;
    }

    if (listen(state.gdb.listen_socket, 1) == -1) {
        LOG_GDB("GDB Server Failed: Could not listen on socket.");
        return;
    }

    state.gdb.server_thread = std::make_shared<std::thread>(server_listen, std::ref(state));

    LOG_GDB("GDB Server is listening on port {}", GDB_SERVER_PORT);
}

void server_close(HostState &state) {
#ifdef _WIN32
    closesocket(state.gdb.listen_socket);
    WSACleanup();
#else
    close(state.gdb.listen_socket);
#endif

    state.gdb.server_die = true;
    if (state.gdb.server_thread && state.gdb.server_thread->get_id() != std::this_thread::get_id())
        state.gdb.server_thread->join();
}

#endif
