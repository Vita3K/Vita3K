#include <memory>

// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <audio/state.h>
#include <config/state.h>
#include <emuenv/state.h>
#include <util/align.h>
#include <util/log.h>

#include <gdbstub/functions.h>
#include <gdbstub/state.h>

#include <cpu/state.h>
#include <io/functions.h>

#include <bit>
#include <cerrno>
#include <iterator>
#include <kernel/state.h>
#include <mem/state.h>
#include <optional>
#include <sstream>
#include <vector>

// Sockets
#ifdef _WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

// Opt-in packet trace, gated by its own config bool so it can be toggled
// without touching the global log level.
#define LOG_GDB_PACKET(state, ...) LOG_INFO_IF((state).cfg.log_gdb_packets, __VA_ARGS__)

// Credit to jfhs for their GDB stub for RPCS3 which this stub is based on.

typedef char PacketData[4096];

// The longest packet the stub waits for before it drops it
constexpr size_t MAX_PACKET_SIZE = 16384;

struct PacketCommand {
    char *data{};
    int64_t length = -1;

    int64_t begin_index = -1;
    int64_t end_index = -1;
    // Decoded (post-unescape) length, which is what handlers see.
    int64_t content_length = -1;
    // On-wire length between '$' and '#', needed to advance past the packet.
    int64_t raw_length = -1;

    char *content_start{};
    char *checksum_start{};

    uint8_t checksum = 0;

    bool is_valid = false;
};

typedef std::function<std::string(EmuEnvState &state, PacketCommand &command)> PacketFunction;

struct PacketFunctionBundle {
    std::string_view name;
    PacketFunction function;
};

static std::string content_string(PacketCommand &command) {
    return std::string(command.content_start, static_cast<unsigned long>(command.content_length));
}

// Hexes
static std::string be_hex(uint32_t value) {
    return fmt::format("{:0>8x}", std::byteswap(value));
}

static std::string to_hex(uint32_t value) {
    return fmt::format("{:0>8x}", value);
}

static std::string to_hex(SceUID value) {
    return fmt::format("{:0>8x}", value);
}

static uint32_t parse_hex(const std::string &hex) {
    return static_cast<uint32_t>(std::strtoul(hex.c_str(), nullptr, 16));
}

// A register value gdb sends, as hex bytes in the target's little endian order
static uint32_t parse_reg(const std::string &hex) {
    return std::byteswap(parse_hex(hex));
}

static uint8_t make_checksum(const char *data, int64_t length) {
    size_t sum = 0;

    for (int64_t a = 0; a < length; a++) {
        sum += data[a];
    }

    return static_cast<uint8_t>(sum % 256);
}

// Parse a single RSP packet of the form `$<body>#cc`, where `<body>` may
// contain `}`-escaped bytes (`}` + (byte ^ 0x20)) for any of the four
// framing bytes $, #, }, *. Decodes escapes in place so handlers see the
// literal bytes gdb intended to send.
static PacketCommand parse_command(char *data, int64_t length) {
    PacketCommand command = {};

    command.data = data;
    command.length = length;

    // Minimum valid packet is "$#cc" (4 bytes), and must start with '$'.
    if (length < 4 || data[0] != '$')
        return command;

    command.begin_index = 1;
    command.content_start = data + 1;

    // Walk forward looking for an unescaped '#'. A '}' byte consumes the
    // following byte as part of an escape sequence.
    int64_t i = 1;
    while (i < length) {
        if (data[i] == '}') {
            if (i + 1 >= length)
                return command; // truncated inside an escape
            i += 2;
            continue;
        }
        if (data[i] == '#')
            break;
        ++i;
    }
    if (i >= length || data[i] != '#')
        return command;

    command.end_index = i;
    command.raw_length = i - command.begin_index;

    // Need two hex checksum chars after '#'.
    if (i + 2 >= length)
        return command;

    command.checksum_start = data + i + 1;
    command.checksum = static_cast<uint8_t>(parse_hex(std::string(command.checksum_start, 2)));

    // Checksum is computed over the on-wire (still-escaped) body bytes.
    if (make_checksum(command.content_start, command.raw_length) != command.checksum)
        return command;

    // Unescape in place: dst always trails src, so this is safe.
    char *src = command.content_start;
    const char *const src_end = data + i;
    char *dst = command.content_start;
    while (src < src_end) {
        if (*src == '}' && (src + 1) < src_end) {
            *dst++ = static_cast<char>(static_cast<uint8_t>(*(src + 1)) ^ 0x20);
            src += 2;
        } else {
            *dst++ = *src++;
        }
    }
    command.content_length = dst - command.content_start;

    command.is_valid = true;
    return command;
}

static int64_t server_reply(GDBState &state, const char *data, int64_t length) {
    uint8_t checksum = make_checksum(data, length);
    std::string packet_data = fmt::format("${}#{:0>2x}", std::string(data, length), checksum);
    size_t sent = 0;
    while (sent < packet_data.size()) {
        const int64_t n = send(state.client_socket, packet_data.data() + sent, static_cast<int>(packet_data.size() - sent), 0);
        // A signal can interrupt send before it writes anything, so try again
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return n;
        sent += static_cast<size_t>(n);
    }
    return static_cast<int64_t>(sent);
}

static int64_t server_reply(GDBState &state, const char *text) {
    return server_reply(state, text, strlen(text));
}

static int64_t server_ack(GDBState &state, char ack = '+') {
    return send(state.client_socket, &ack, 1, 0);
}

static std::string cmd_supported(EmuEnvState &state, PacketCommand &command) {
    return "multiprocess-;swbreak+;hwbreak-;qRelocInsn-;fork-events-;vfork-events-;"
           "exec-events-;vContSupported+;QThreadEvents-;no-resumed-;qXfer:features:read+;"
           "qXfer:libraries:read+";
}

// Register numbers in the target description and in remote packets.
constexpr uint32_t REG_CPSR = 16;
constexpr uint32_t REG_D0 = 17;
constexpr uint32_t VFP_DOUBLE_REGS = 32;
constexpr uint32_t REG_FPSCR = REG_D0 + VFP_DOUBLE_REGS;
constexpr uint32_t REG_Q0 = REG_FPSCR + 1;
constexpr uint32_t NEON_QUAD_REGS = 16;

// Core, VFP and NEON registers. gdb shows s0-s31 from the d registers by itself but not q0-q15, so
// the q registers are listed too.
static std::string make_target_xml() {
    std::string xml = R"(<?xml version="1.0"?>
<!DOCTYPE target SYSTEM "gdb-target.dtd">
<target version="1.0">
<architecture>arm</architecture>
<feature name="org.gnu.gdb.arm.core">
)";
    for (int r = 0; r < 13; r++)
        fmt::format_to(std::back_inserter(xml), R"(<reg name="r{}" bitsize="32" type="uint32"/>)", r);
    xml += fmt::format(R"(<reg name="sp" bitsize="32" type="data_ptr"/>
<reg name="lr" bitsize="32" type="uint32"/>
<reg name="pc" bitsize="32" type="code_ptr"/>
<reg name="cpsr" bitsize="32" type="uint32" regnum="{}"/>
</feature>
<feature name="org.gnu.gdb.arm.vfp">
)",
        REG_CPSR);
    for (uint32_t d = 0; d < VFP_DOUBLE_REGS; d++)
        fmt::format_to(std::back_inserter(xml), R"(<reg name="d{}" bitsize="64" type="ieee_double" regnum="{}"/>)", d, REG_D0 + d);
    xml += fmt::format(R"(<reg name="fpscr" bitsize="32" type="int" group="float" regnum="{}"/>
</feature>
<feature name="org.gnu.gdb.arm.neon">
<vector id="v4u32" type="uint32" count="4"/>
)",
        REG_FPSCR);
    for (uint32_t q = 0; q < NEON_QUAD_REGS; q++)
        fmt::format_to(std::back_inserter(xml), R"(<reg name="q{}" bitsize="128" type="v4u32" regnum="{}"/>)", q, REG_Q0 + q);
    xml += "</feature>\n</target>";
    return xml;
}

static const std::string TARGET_XML = make_target_xml();

// qXfer:features:read:target.xml:OFFSET,LENGTH returns the part of TARGET_XML gdb asks for, with m
// before it while more follows and l before the last part
static std::string cmd_target_xml(EmuEnvState &state, PacketCommand &command) {
    const std::string content = content_string(command);
    const size_t comma = content.find(',');
    const size_t colon = content.rfind(':', comma);
    if (colon == std::string::npos || comma == std::string::npos)
        return "E00";
    const size_t offset = parse_hex(content.substr(colon + 1, comma - colon - 1));
    const size_t length = parse_hex(content.substr(comma + 1));
    if (offset >= TARGET_XML.size())
        return "l";
    const std::string part = TARGET_XML.substr(offset, length);
    return (offset + part.size() < TARGET_XML.size() ? "m" : "l") + part;
}

static std::string cmd_xfer_libraries(EmuEnvState &state, PacketCommand &command) {
    const std::string content = content_string(command);
    const std::string prefix = "qXfer:libraries:read::";
    if (content.substr(0, prefix.size()) != prefix)
        return "E00";

    const std::string params = content.substr(prefix.size());
    const size_t comma = params.find(',');
    const uint32_t offset = parse_hex(params.substr(0, comma));
    const uint32_t length = parse_hex(params.substr(comma + 1));

    std::string xml = "<library-list>";
    {
        const auto guard = std::lock_guard(state.kernel.mutex);
        for (const auto &[uid, module] : state.kernel.loaded_modules) {
            const uint32_t base = module->info.segments[0].vaddr.address();
            xml += fmt::format("<library name=\"{}\"><segment address=\"{:#x}\"/></library>",
                module->info.module_name, base);
        }
    }
    xml += "</library-list>";

    if (offset >= xml.size())
        return "l";

    const std::string chunk = xml.substr(offset, length);
    const bool last = (offset + chunk.size()) >= xml.size();
    return (last ? "l" : "m") + chunk;
}

static std::string cmd_reply_empty(EmuEnvState &state, PacketCommand &command) {
    return "";
}

// This function is not thread safe
static SceUID select_thread(EmuEnvState &state, int thread_id) {
    if (thread_id == 0) {
        if (state.kernel.threads.empty())
            return -1;
        return state.kernel.threads.begin()->first;
    }
    return thread_id;
}

static std::string cmd_set_current_thread(EmuEnvState &state, PacketCommand &command) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    int32_t thread_id = parse_hex(std::string(
        command.content_start + 2, static_cast<unsigned long>(command.content_length - 2)));

    switch (command.content_start[1]) {
    case 'c':
        LOG_GDB_PACKET(state, "GDB Server Deprecated Continue Option 'c'");
        // state.gdb.current_continue_thread = select_thread(state, thread_id);
        break;
    case 'g':
        state.gdb.current_thread = select_thread(state, thread_id);
        break;
    default:
        LOG_WARN("GDB Server Unknown Set Current Thread OP. {}", command.content_start[1]);
        break;
    }

    return "OK";
}

static std::string cmd_get_current_thread(EmuEnvState &state, PacketCommand &command) {
    return "QC" + to_hex(state.gdb.current_thread);
}

static void modify_reg(CPUState &state, uint32_t reg, uint32_t value) {
    if (reg <= 12) {
        state.set_reg(reg, value);
        return;
    }

    if (reg == 13) {
        state.set_sp(value);
        return;
    }
    if (reg == 14) {
        state.set_lr(value);
        return;
    }
    if (reg == 15) {
        state.set_pc(value);
        return;
    }

    if (reg == REG_FPSCR) {
        state.set_fpscr(value);
        return;
    }
    if (reg == REG_CPSR) {
        state.set_cpsr(value);
        return;
    }

    LOG_WARN("GDB Server Modified Invalid Register {}", reg);
}

// How many 32-bit words a register has in p and P packets
static size_t reg_words(uint32_t reg) {
    if (reg >= REG_D0 && reg < REG_D0 + VFP_DOUBLE_REGS)
        return 2;
    if (reg >= REG_Q0 && reg < REG_Q0 + NEON_QUAD_REGS)
        return 4;
    return 1;
}

// The context word that a register starts at, or nothing for an unknown register
static std::optional<uint32_t> reg_first_word(uint32_t reg) {
    if (reg <= 15)
        return reg;
    if (reg == REG_CPSR)
        return CPUContext::WORD_CPSR;
    if (reg >= REG_D0 && reg < REG_D0 + VFP_DOUBLE_REGS)
        return CPUContext::WORD_S0 + (reg - REG_D0) * 2;
    if (reg == REG_FPSCR)
        return CPUContext::WORD_FPSCR;
    if (reg >= REG_Q0 && reg < REG_Q0 + NEON_QUAD_REGS)
        return CPUContext::WORD_S0 + (reg - REG_Q0) * 4;
    return std::nullopt;
}

static void write_float_regs(CPUState &cpu, size_t first, size_t count, const std::string &hex) {
    for (size_t i = 0; i < count; i++)
        cpu.set_float_reg(first + i, std::bit_cast<float>(parse_reg(hex.substr(i * 8, 8))));
}

static std::string cmd_read_registers(EmuEnvState &state, PacketCommand &command) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    if (state.gdb.current_thread == -1
        || !state.kernel.threads.contains(state.gdb.current_thread))
        return "E00";

    const CPUContext context = state.kernel.threads[state.gdb.current_thread]->cpu->get_context();

    // gdb reads the registers after cpsr one by one with p
    std::string str;
    str.reserve((CPUContext::WORD_CPSR + 1) * 8);
    for (uint32_t i = 0; i <= CPUContext::WORD_CPSR; i++)
        str += be_hex(context.word(i));
    return str;
}

static std::string cmd_write_registers(EmuEnvState &state, PacketCommand &command) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    if (state.gdb.current_thread == -1
        || !state.kernel.threads.contains(state.gdb.current_thread))
        return "E00";

    CPUState &cpu = *state.kernel.threads[state.gdb.current_thread]->cpu.get();

    const std::string content = content_string(command).substr(1);

    for (uint32_t a = 0; a <= REG_CPSR && a < content.size() / 8; a++)
        modify_reg(cpu, a, parse_reg(content.substr(a * 8, 8)));

    return "OK";
}

static std::string cmd_read_register(EmuEnvState &state, PacketCommand &command) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    if (state.gdb.current_thread == -1
        || !state.kernel.threads.contains(state.gdb.current_thread))
        return "E00";

    const CPUContext context = state.kernel.threads[state.gdb.current_thread]->cpu->get_context();

    const std::string content = content_string(command);
    const uint32_t reg = parse_hex(content.substr(1));
    const std::optional<uint32_t> first = reg_first_word(reg);
    if (!first) {
        LOG_WARN("GDB Server Queried Invalid Register {}", reg);
        return be_hex(0);
    }
    // A double register is two words and a quad register four, the low one first
    std::string result;
    for (uint32_t i = 0; i < reg_words(reg); i++)
        result += be_hex(context.word(*first + i));
    return result;
}

static std::string cmd_write_register(EmuEnvState &state, PacketCommand &command) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    if (state.gdb.current_thread == -1
        || !state.kernel.threads.contains(state.gdb.current_thread))
        return "E00";

    CPUState &cpu = *state.kernel.threads[state.gdb.current_thread]->cpu.get();

    const std::string content = content_string(command);
    const size_t equal_index = content.find('=');
    const uint32_t reg = parse_hex(content.substr(1, equal_index - 1));
    const std::string value = content.substr(equal_index + 1);
    if (value.size() != reg_words(reg) * 8)
        return "E00";
    if (reg >= REG_D0 && reg < REG_D0 + VFP_DOUBLE_REGS)
        write_float_regs(cpu, (reg - REG_D0) * 2, 2, value);
    else if (reg >= REG_Q0 && reg < REG_Q0 + NEON_QUAD_REGS)
        write_float_regs(cpu, (reg - REG_Q0) * 4, 4, value);
    else
        modify_reg(cpu, reg, parse_reg(value));

    return "OK";
}

static bool check_memory_region(Address address, Address length, MemState &mem) {
    if (!address) {
        return false;
    }

    Address it = address;
    bool valid = true;
    for (; it < address + length; it += mem.host_page_size) {
        if (!is_valid_addr(mem, it)) {
            valid = false;
            break;
        }
    }
    return valid;
}

static std::string cmd_read_memory(EmuEnvState &state, PacketCommand &command) {
    const std::string content = content_string(command);
    const size_t pos = content.find(',');

    const std::string first = content.substr(1, pos - 1);
    const std::string second = content.substr(pos + 1);
    const uint32_t address = parse_hex(first);
    const uint32_t length = parse_hex(second);

    if (!check_memory_region(address, length, state.mem))
        return "EAA";

    std::string str;
    str.reserve(length * 2);

    for (uint32_t a = 0; a < length; a++) {
        fmt::format_to(std::back_inserter(str), "{:02x}", *Ptr<uint8_t>(address + a).get(state.mem));
    }

    return str;
}

static std::string cmd_write_memory(EmuEnvState &state, PacketCommand &command) {
    const std::string content = content_string(command);
    const size_t pos_first = content.find(',');
    const size_t pos_second = content.find(':');

    const std::string first = content.substr(1, pos_first - 1);
    const std::string second = content.substr(pos_first + 1, pos_second - pos_first);
    const uint32_t address = parse_hex(first);
    const uint32_t length = parse_hex(second);
    const std::string hex_data = content.substr(pos_second + 1);

    if (!check_memory_region(address, length, state.mem))
        return "EAA";

    for (uint32_t a = 0; a < length; a++) {
        *Ptr<uint8_t>(address + a).get(state.mem) = static_cast<uint8_t>(parse_hex(hex_data.substr(a * 2, 2)));
    }

    return "OK";
}

static std::string cmd_write_binary(EmuEnvState &state, PacketCommand &command) {
    const std::string content = content_string(command);
    const size_t pos_first = content.find(',');
    const size_t pos_second = content.find(':');

    const std::string first = content.substr(1, pos_first - 1);
    const std::string second = content.substr(pos_first + 1, pos_second - pos_first);
    const uint32_t address = parse_hex(first);
    const uint32_t length = parse_hex(second);
    const char *data = command.content_start + pos_second + 1;

    if (!check_memory_region(address, length, state.mem))
        return "EAA";

    for (uint32_t a = 0; a < length; a++) {
        *Ptr<uint8_t>(address + a).get(state.mem) = data[a];
    }

    return "OK";
}

static std::string cmd_detach(EmuEnvState &state, PacketCommand &command) {
    state.kernel.debugger.remove_all_breakpoints(state.mem);
    state.kernel.set_threads_suspended(false);

    state.gdb.current_thread = 0;
    state.audio.switch_state(false);
    LOG_INFO("GDB Server Detached — breakpoints removed, threads resumed.");
    return "OK";
}

static std::string log_stack_traceback(EmuEnvState &state, CPUState &cpu) {
    std::string str;
    const Address sp = cpu.get_sp();
    for (Address addr = sp; addr <= sp + 1024; addr += 4) {
        if (!Ptr<uint32_t>(addr).valid(state.mem))
            continue;
        const Address value = *Ptr<uint32_t>(addr).get(state.mem);
        if (const auto mod = state.kernel.find_module_by_addr(value))
            fmt::format_to(std::back_inserter(str), "0x{:X} (module: {})\n", value, mod->module_name);
    }
    return str;
}

// The stop reply for a thread that stopped on a trap
static std::string stop_reply(SceUID thread_id) {
    return "T05thread:" + to_hex(thread_id) + ";";
}

static std::vector<ThreadStatePtr> list_threads(EmuEnvState &state) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    std::vector<ThreadStatePtr> threads;
    for (const auto &[_, thread] : state.kernel.threads)
        threads.push_back(thread);
    return threads;
}

static std::string cmd_continue(EmuEnvState &state, PacketCommand &command) {
    const std::string content = content_string(command);

    uint64_t index = 5;
    uint64_t next = 0;
    do {
        next = content.find(';', index + 1);
        std::string text = content.substr(index + 1, next - index - 1);

        const char cmd = text[0];
        switch (cmd) {
        case 's':
        case 'S':
        case 'r': {
            // Treat range-stepping (r<start>,<end>) as a single instruction
            // step. gdb will re-issue vCont;r until PC leaves the range.
            // The action names the thread to step, else it's the current one
            const size_t colon = text.find(':');
            SceUID thread_id = colon == std::string::npos ? 0 : static_cast<SceUID>(parse_hex(text.substr(colon + 1)));
            if (thread_id <= 0)
                thread_id = state.gdb.current_thread;
            const ThreadStatePtr thread = state.kernel.get_thread(thread_id);
            if (!thread)
                return "E00";

            // A thread something else holds can't step, and gdb takes the error as a stop
            // TODO: a step into a call that waits on a thread the debugger holds never returns
            if (!thread->step())
                return "E01";

            state.gdb.current_thread = thread_id;
            return stop_reply(thread_id);
        }
        case 'c':
        case 'C': {
            // Resume the world. A thread something else holds stays stopped.
            state.kernel.debugger.has_break = false;
            state.kernel.set_threads_suspended(false);
            state.audio.switch_state(false);

            // Wait for a breakpoint hit (condvar) or a Ctrl-C interrupt
            // (\x03 byte on the socket). The condvar fires instantly on
            // breakpoint; the 100ms timeout is only for the interrupt poll.
            SceUID stopped_id = 0;
            bool did_break = false;
            bool interrupted = false;
            while (!did_break && !interrupted && !state.gdb.server_die) {
                {
                    std::unique_lock<std::mutex> bk_lock(state.kernel.debugger.break_mutex);
                    state.kernel.debugger.break_cv.wait_for(bk_lock, std::chrono::milliseconds(100), [&]() {
                        return state.kernel.debugger.has_break.load() || state.gdb.server_die;
                    });
                }

                if (state.kernel.debugger.has_break.load()) {
                    stopped_id = state.kernel.debugger.break_thread_id;
                    state.kernel.debugger.has_break = false;
                    did_break = true;
                }

                // Check for gdb interrupt (Ctrl-C sends raw \x03).
                if (!did_break) {
                    fd_set read_set;
                    timeval tv = { 0, 0 };
                    FD_ZERO(&read_set);
                    FD_SET(state.gdb.client_socket, &read_set);
                    if (select(state.gdb.client_socket + 1, &read_set, nullptr, nullptr, &tv) > 0) {
                        char byte;
                        if (recv(state.gdb.client_socket, &byte, 1, MSG_PEEK) == 1 && byte == '\x03') {
                            recv(state.gdb.client_socket, &byte, 1, 0);
                            interrupted = true;
                            LOG_INFO("GDB interrupt received (Ctrl-C)");
                            auto lock = std::unique_lock(state.kernel.mutex);
                            if (!state.kernel.threads.empty())
                                stopped_id = state.kernel.threads.begin()->first;
                        }
                    }
                }
            }

            if (state.gdb.server_die)
                return "";

            if (did_break) {
                const ThreadStatePtr stopped = state.kernel.get_thread(stopped_id);
                LOG_INFO("GDB Breakpoint trigger (thread name: {}, thread_id: {})", stopped->name, stopped->id);
                LOG_INFO("PC: {} LR: {}", stopped->cpu->get_pc(), stopped->cpu->get_lr());
                LOG_INFO("{}", log_stack_traceback(state, *stopped->cpu));
            }

            // Stop the world. A thread inside a kernel call stops when the call returns.
            state.kernel.set_threads_suspended(true);
            for (const ThreadStatePtr &thread : list_threads(state))
                thread->wait_until_not_running();
            state.audio.switch_state(true);

            state.gdb.current_thread = stopped_id;
            return stop_reply(stopped_id);
        }
        default:
            LOG_WARN("Unsupported vCont command '{}'", cmd);
            break;
        }

        index = next;
    } while (next != std::string::npos);

    return "";
}

static std::string cmd_continue_supported(EmuEnvState &state, PacketCommand &command) {
    return "vCont;c;C;s;S;t;r";
}

static std::string cmd_thread_alive(EmuEnvState &state, PacketCommand &command) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    const std::string content = content_string(command);
    const int32_t thread_id = parse_hex(content.substr(1));

    // Assuming a thread is removed from the map when it closes or is killed.
    if (state.kernel.threads.contains(thread_id))
        return "OK";

    return "E00";
}

static std::string cmd_kill(EmuEnvState &state, PacketCommand &command) {
    return "OK";
}

static std::string cmd_die(EmuEnvState &state, PacketCommand &command) {
    state.gdb.server_die = true;
    return "";
}

static std::string cmd_attached(EmuEnvState &state, PacketCommand &command) { return "1"; }

static std::string cmd_offsets(EmuEnvState &state, PacketCommand &command) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    if (state.kernel.loaded_modules.empty())
        return "";

    // The first loaded module is the main executable (eboot.bin).
    // TextSeg/DataSeg format gives gdb the absolute segment addresses;
    // gdb computes the relocation offsets from the ELF headers internally.
    const auto &module = state.kernel.loaded_modules.begin()->second;
    uint32_t text_addr = module->info.segments[0].vaddr.address();
    uint32_t data_addr = module->info.segments[1].vaddr.address();

    if (data_addr)
        return fmt::format("TextSeg={:x};DataSeg={:x}", text_addr, data_addr);
    return fmt::format("TextSeg={:x}", text_addr);
}

static std::string cmd_thread_status(EmuEnvState &state, PacketCommand &command) { return "T0"; }

static std::string cmd_reason(EmuEnvState &state, PacketCommand &command) { return "S05"; }

static std::string cmd_get_first_thread(EmuEnvState &state, PacketCommand &command) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    state.gdb.thread_info_index = 0;

    return 'm' + to_hex(state.kernel.threads.begin()->first);
}

static std::string cmd_get_next_thread(EmuEnvState &state, PacketCommand &command) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    std::string str;

    ++state.gdb.thread_info_index;
    if (state.gdb.thread_info_index == state.kernel.threads.size()) {
        str += 'l';
    } else {
        auto iter = state.kernel.threads.begin();
        std::advance(iter, state.gdb.thread_info_index);

        str += 'm';
        str += to_hex(iter->first);
    }

    return str;
}

static std::string cmd_add_breakpoint(EmuEnvState &state, PacketCommand &command) {
    const std::string content = content_string(command);

    const uint64_t first = content.find(',');
    const uint64_t second = content.find(',', first + 1);
    const uint32_t type = static_cast<uint32_t>(std::stol(content.substr(1, first - 1)));
    // GDB may set bit 0 of the address to indicate Thumb state; mask it so the
    // breakpoint lands on the actual instruction boundary.
    const uint32_t address = align_down(parse_hex(content.substr(first + 1, second - 1 - first)), 2);
    const uint32_t kind = static_cast<uint32_t>(std::stol(content.substr(second + 1, content.size() - second - 1)));

    // Only Z0 (software breakpoint) is supported. Returning empty for Z1-Z4
    // tells gdb to fall back to software watchpoints instead of letting it
    // corrupt the target memory by patching BKPT at a data address.
    if (type != 0)
        return "";

    const uint32_t bp_size = (kind == 2 || kind == 3) ? 2 : 4;
    if (!check_memory_region(address, bp_size, state.mem)) {
        LOG_WARN("GDB Server Breakpoint rejected — address {} not mapped.", log_hex(address));
        return "E01";
    }

    LOG_INFO("GDB Server New Breakpoint at {} ({}, {}).", log_hex(address), type, kind);

    // kind is 2 for a 16-bit Thumb instruction, 3 for a 32-bit one and 4 for ARM. A 16-bit BKPT
    // over the first halfword stops at a 32-bit Thumb instruction too.
    // https://sourceware.org/gdb/current/onlinedocs/gdb/ARM-Breakpoint-Kinds.html#ARM-Breakpoint-Kinds
    state.kernel.debugger.add_breakpoint(state.mem, address, kind == 2 || kind == 3);

    return "OK";
}

static std::string cmd_remove_breakpoint(EmuEnvState &state, PacketCommand &command) {
    const std::string content = content_string(command);

    const uint64_t first = content.find(',');
    const uint64_t second = content.find(',', first + 1);
    const uint32_t type = static_cast<uint32_t>(std::stol(content.substr(1, first - 1)));
    // Mirror cmd_add_breakpoint: strip the GDB Thumb-state bit so the lookup
    // key matches the address used at insertion.
    const uint32_t address = align_down(parse_hex(content.substr(first + 1, second - 1 - first)), 2);
    const uint32_t kind = static_cast<uint32_t>(std::stol(content.substr(second + 1, content.size() - second - 1)));

    // Match cmd_add_breakpoint: only Z0 is handled.
    if (type != 0)
        return "";

    LOG_INFO("GDB Server Removed Breakpoint at {} ({}, {}).", log_hex(address), type, kind);
    state.kernel.debugger.remove_breakpoint(state.mem, address);

    return "OK";
}

// GDB's Host I/O (vFile) flag values, from include/gdb/fileio.h.
// These are the GDB-protocol values, NOT host POSIX values.
constexpr int GDB_O_RDONLY = 0x0;
constexpr int GDB_O_WRONLY = 0x1;
constexpr int GDB_O_RDWR = 0x2;
constexpr int GDB_O_APPEND = 0x8;
constexpr int GDB_O_CREAT = 0x200;
constexpr int GDB_O_TRUNC = 0x400;
constexpr int GDB_O_EXCL = 0x800;

constexpr int GDB_ENOENT = 2;
constexpr int GDB_EBADF = 9;
constexpr int GDB_EINVAL = 22;

static int gdb_flags_to_sce(int gdb_flags) {
    int sce = 0;
    const int access = gdb_flags & 0x3;
    if (access == GDB_O_WRONLY)
        sce |= SCE_O_WRONLY;
    else if (access == GDB_O_RDWR)
        sce |= SCE_O_RDWR;
    else
        sce |= SCE_O_RDONLY;
    if (gdb_flags & GDB_O_APPEND)
        sce |= SCE_O_APPEND;
    if (gdb_flags & GDB_O_CREAT)
        sce |= SCE_O_CREAT;
    if (gdb_flags & GDB_O_TRUNC)
        sce |= SCE_O_TRUNC;
    if (gdb_flags & GDB_O_EXCL)
        sce |= SCE_O_EXCL;
    return sce;
}

static std::string vfile_unhex_path(const std::string &hex) {
    std::string out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        const unsigned v = static_cast<unsigned>(std::stoul(hex.substr(i, 2), nullptr, 16));
        out.push_back(static_cast<char>(v));
    }
    return out;
}

// Escape the four RSP framing bytes ($, #, }, *) in a binary attachment.
// Null bytes pass through unchanged — callers must use the length-taking
// server_reply path so they aren't truncated by strlen.
static std::string vfile_escape_binary(const char *data, size_t len) {
    std::string out;
    out.reserve(len);
    for (size_t i = 0; i < len; i++) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        if (c == '$' || c == '#' || c == '}' || c == '*') {
            out.push_back('}');
            out.push_back(static_cast<char>(c ^ 0x20));
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

static std::string vfile_error(int err) {
    return fmt::format("F-1,{:x}", err);
}

static void vfile_put32_be(char *dst, uint32_t v) {
    dst[0] = static_cast<char>((v >> 24) & 0xff);
    dst[1] = static_cast<char>((v >> 16) & 0xff);
    dst[2] = static_cast<char>((v >> 8) & 0xff);
    dst[3] = static_cast<char>(v & 0xff);
}

static void vfile_put64_be(char *dst, uint64_t v) {
    for (int i = 0; i < 8; i++)
        dst[i] = static_cast<char>((v >> (56 - 8 * i)) & 0xff);
}

static std::string cmd_vfile(EmuEnvState &state, PacketCommand &command) {
    const std::string content = content_string(command);
    // strip "vFile:" prefix
    const std::string sub = content.substr(6);

    // setfs:<pid>  — we only have one filesystem, accept any pid as success.
    if (sub.compare(0, 6, "setfs:") == 0) {
        return "F0";
    }

    // open:<pathname-hex>,<flags-hex>,<mode-hex>
    if (sub.compare(0, 5, "open:") == 0) {
        const std::string args = sub.substr(5);
        const size_t c1 = args.find(',');
        const size_t c2 = args.find(',', c1 + 1);
        if (c1 == std::string::npos || c2 == std::string::npos)
            return vfile_error(GDB_EINVAL);
        const std::string path = vfile_unhex_path(args.substr(0, c1));
        const int gdb_flags = static_cast<int>(std::stoul(args.substr(c1 + 1, c2 - c1 - 1), nullptr, 16));
        // mode bits ignored — Vita3K has no concept of host mode bits
        const int sce_flags = gdb_flags_to_sce(gdb_flags);
        const SceUID fd = open_file(state.io, path.c_str(), sce_flags, state.vita_fs_path, "gdbstub");
        if (fd < 0) {
            LOG_INFO("GDB vFile open failed: '{}'", path);
            return vfile_error(GDB_ENOENT);
        }
        LOG_INFO("GDB vFile opened: '{}' fd={}", path, fd);
        return fmt::format("F{:x}", fd);
    }

    // close:<fd-hex>
    if (sub.compare(0, 6, "close:") == 0) {
        const SceUID fd = static_cast<SceUID>(std::stoul(sub.substr(6), nullptr, 16));
        const int rc = close_file(state.io, fd, "gdbstub");
        if (rc < 0)
            return vfile_error(GDB_EBADF);
        return "F0";
    }

    // pread:<fd-hex>,<count-hex>,<offset-hex>
    if (sub.compare(0, 6, "pread:") == 0) {
        const std::string args = sub.substr(6);
        const size_t c1 = args.find(',');
        const size_t c2 = args.find(',', c1 + 1);
        if (c1 == std::string::npos || c2 == std::string::npos)
            return vfile_error(GDB_EINVAL);
        const SceUID fd = static_cast<SceUID>(std::stoul(args.substr(0, c1), nullptr, 16));
        uint32_t count = static_cast<uint32_t>(std::stoul(args.substr(c1 + 1, c2 - c1 - 1), nullptr, 16));
        const uint64_t offset = std::stoull(args.substr(c2 + 1), nullptr, 16);

        // Cap to fit in PacketData (4096) with reply header + worst-case 2x
        // escaping headroom.
        constexpr uint32_t MAX_PREAD = 1024;
        if (count > MAX_PREAD)
            count = MAX_PREAD;

        if (seek_file(fd, static_cast<SceOff>(offset), SCE_SEEK_SET, state.io, "gdbstub") < 0)
            return vfile_error(GDB_EBADF);

        std::vector<char> buf(count);
        const int n = read_file(buf.data(), state.io, fd, count, "gdbstub");
        if (n < 0)
            return vfile_error(GDB_EBADF);

        return fmt::format("F{:x};", n) + vfile_escape_binary(buf.data(), static_cast<size_t>(n));
    }

    // fstat:<fd-hex>
    if (sub.compare(0, 6, "fstat:") == 0) {
        const SceUID fd = static_cast<SceUID>(std::stoul(sub.substr(6), nullptr, 16));
        SceIoStat sce_stat{};
        if (stat_file_by_fd(state.io, fd, &sce_stat, state.vita_fs_path, "gdbstub") < 0)
            return vfile_error(GDB_EBADF);

        // GDB's struct stat is a fixed 64-byte big-endian layout.
        // See "struct stat" in the gdb manual, Host I/O Packets section.
        char st[64] = {};
        vfile_put32_be(st + 0, 0); // dev
        vfile_put32_be(st + 4, 0); // ino
        vfile_put32_be(st + 8, 0100644); // mode: regular file, rw-r--r--
        vfile_put32_be(st + 12, 1); // nlink
        vfile_put32_be(st + 16, 0); // uid
        vfile_put32_be(st + 20, 0); // gid
        vfile_put32_be(st + 24, 0); // rdev
        vfile_put64_be(st + 28, static_cast<uint64_t>(sce_stat.st_size)); // size
        vfile_put64_be(st + 36, 4096); // blksize
        vfile_put64_be(st + 44, (static_cast<uint64_t>(sce_stat.st_size) + 511) / 512); // blocks
        vfile_put32_be(st + 52, 0); // atime
        vfile_put32_be(st + 56, 0); // mtime
        vfile_put32_be(st + 60, 0); // ctime

        return fmt::format("F{:x};", 64) + vfile_escape_binary(st, 64);
    }

    LOG_GDB_PACKET(state, "GDB Server: Unimplemented vFile op. {}", content);
    return "";
}

static std::string cmd_deprecated(EmuEnvState &state, PacketCommand &command) {
    LOG_GDB_PACKET(state, "GDB Server: Deprecated Packet. {}", content_string(command));

    return "";
}

static std::string cmd_unimplemented(EmuEnvState &state, PacketCommand &command) {
    LOG_GDB_PACKET(state, "GDB Server: Unimplemented Packet. {}", content_string(command));

    return "";
}

const static PacketFunctionBundle functions[] = {
    // General
    { "!", cmd_unimplemented },
    { "?", cmd_reason },
    { "H", cmd_set_current_thread },
    { "T", cmd_thread_alive },
    { "i", cmd_unimplemented },
    { "I", cmd_unimplemented },
    { "A", cmd_unimplemented },
    { "bc", cmd_unimplemented },
    { "bs", cmd_unimplemented },
    { "t", cmd_unimplemented },

    // Read/Write
    { "p", cmd_read_register },
    { "P", cmd_write_register },
    { "g", cmd_read_registers },
    { "G", cmd_write_registers },
    { "m", cmd_read_memory },
    { "M", cmd_write_memory },
    { "X", cmd_write_binary },

    // Query Packets
    { "qXfer:libraries:read:", cmd_xfer_libraries },
    { "qfThreadInfo", cmd_get_first_thread },
    { "qsThreadInfo", cmd_get_next_thread },
    { "qSupported", cmd_supported },
    { "qXfer:features:read:target.xml:", cmd_target_xml },
    { "qOffsets", cmd_offsets },
    { "qAttached", cmd_attached },
    { "qTStatus", cmd_thread_status },
    { "qC", cmd_get_current_thread },
    { "q", cmd_unimplemented },
    { "Q", cmd_unimplemented },

    // Shutdown
    { "D", cmd_detach },
    { "d", cmd_unimplemented },
    { "r", cmd_unimplemented },
    { "R", cmd_unimplemented },
    { "k", cmd_die },

    // Control Packets
    { "vCont?", cmd_continue_supported },
    { "vCont", cmd_continue },
    { "vKill", cmd_kill },
    { "vMustReplyEmpty", cmd_reply_empty },
    { "vFile:", cmd_vfile },
    { "v", cmd_unimplemented },

    // Breakpoints
    { "z", cmd_remove_breakpoint },
    { "Z", cmd_add_breakpoint },

    // Deprecated
    { "b", cmd_deprecated },
    { "B", cmd_deprecated },
    { "c", cmd_deprecated },
    { "C", cmd_deprecated },
    { "s", cmd_deprecated },
    { "S", cmd_deprecated },
};

static bool command_begins_with(PacketCommand &command, const std::string_view small_str) {
    // If the command's content is shorter than small_str, it can't match
    if (static_cast<size_t>(command.content_length) < small_str.size())
        return false;

    return std::memcmp(command.content_start, small_str.data(), small_str.size()) == 0;
}

static int64_t server_next(EmuEnvState &state) {
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

    const int64_t length = recv(state.gdb.client_socket, buffer, sizeof(buffer), 0);
    if (length <= 0) {
        LOG_INFO("GDB Server Connection Closed");
        return -1;
    }
    state.gdb.input_buffer.append(buffer, static_cast<size_t>(length));
    auto &input = state.gdb.input_buffer;
    while (!input.empty()) {
        switch (input.front()) {
        case '+': {
            input.erase(0, 1);
            break; // Cool.
        }
        case '-': {
            input.erase(0, 1);
            LOG_WARN("GDB Server Transmission Error");
            server_reply(state.gdb, state.gdb.last_reply.data(), state.gdb.last_reply.size());
            break;
        }
        case '$': {
            const size_t end = input.find('#');
            // Wait for the rest of a packet split across reads, up to a limit
            if (end == std::string::npos || input.size() < end + 3) {
                if (input.size() > MAX_PACKET_SIZE) {
                    LOG_WARN("GDB Server Packet Too Long");
                    input.clear();
                    server_ack(state.gdb, '-');
                }
                return length;
            }
            const size_t packet_size = end + 3;
            PacketCommand command = parse_command(input.data(), static_cast<int64_t>(packet_size));
            if (command.is_valid) {
                server_ack(state.gdb, '+');
                bool found_command = false;
                for (const auto &function : functions) {
                    if (command_begins_with(command, function.name)) {
                        found_command = true;
                        LOG_GDB_PACKET(state, "GDB Server Recognized Command as {}. {}", function.name,
                            std::string(command.content_start, command.content_length));
                        state.gdb.last_reply = function.function(state, command);
                        if (state.gdb.server_die)
                            break;
                        server_reply(state.gdb, state.gdb.last_reply.data(), state.gdb.last_reply.size());
                        break;
                    }
                }
                if (!found_command) {
                    LOG_GDB_PACKET(state, "GDB Server Unrecognized Command. {}", std::string(command.content_start, command.content_length));
                    state.gdb.last_reply = "";
                    server_reply(state.gdb, state.gdb.last_reply.data(), state.gdb.last_reply.size());
                }
            } else {
                server_ack(state.gdb, '-');
                LOG_WARN("GDB Server Invalid Command. {}", input.substr(0, packet_size));
            }
            input.erase(0, packet_size);
            break;
        }
        default:
            input.erase(0, 1);
            break;
        }
        if (state.gdb.server_die)
            break;
    }

    return length;
}

static void server_listen(EmuEnvState &state) {
    while (!state.gdb.server_die) {
        state.gdb.client_socket = accept(state.gdb.listen_socket, nullptr, nullptr);
        state.gdb.input_buffer.clear();

        if (state.gdb.client_socket == -1) {
            if (!state.gdb.server_die)
                LOG_ERROR("GDB Server Failed: Could not accept socket.");
            break;
        }

        LOG_INFO("GDB Server Received Connection");

        // A reply goes out at once, without waiting for the ack of the '+' sent before it
        const int nodelay = 1;
        setsockopt(state.gdb.client_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&nodelay), sizeof(nodelay));

        int64_t status;
        do {
            status = server_next(state);
        } while (status >= 0 && !state.gdb.server_die);

#ifdef _WIN32
        closesocket(state.gdb.client_socket);
#else
        close(state.gdb.client_socket);
#endif
        state.gdb.client_socket = BAD_SOCK;

        if (!state.gdb.server_die)
            LOG_INFO("GDB Server Client Disconnected, waiting for new connection...");
    }

    server_close(state);
}

void server_open(EmuEnvState &state) {
    LOG_INFO("Starting GDB Server...");

#ifdef _WIN32
    int32_t err = WSAStartup(MAKEWORD(2, 2), &state.gdb.wsaData);
    if (err) {
        LOG_ERROR("GDB Server Failed: Could not start WSA service.");
        return;
    }
#endif

    state.gdb.listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (state.gdb.listen_socket == -1) {
        LOG_ERROR("GDB Server Failed: Could not create socket.");
        return;
    }

#ifndef _WIN32
    // An earlier session's connection keeps the port in TIME_WAIT for a while after it ends
    const int reuse = 1;
    setsockopt(state.gdb.listen_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

    sockaddr_in socket_address{};
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(GDB_SERVER_PORT);
#ifdef _WIN32
    socket_address.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
#else
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
#endif

    if (bind(state.gdb.listen_socket, (sockaddr *)&socket_address, sizeof(socket_address)) == -1) {
        LOG_ERROR("GDB Server Failed: Could not bind socket.");
        return;
    }

    if (listen(state.gdb.listen_socket, 1) == -1) {
        LOG_ERROR("GDB Server Failed: Could not listen on socket.");
        return;
    }

    state.gdb.server_thread = std::make_shared<std::thread>(server_listen, std::ref(state));

    LOG_INFO("GDB Server is listening on port {}", GDB_SERVER_PORT);
}

void server_close(EmuEnvState &state) {
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
