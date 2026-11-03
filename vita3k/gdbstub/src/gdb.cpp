#include <algorithm>
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
#include <ctrl/state.h>
#include <display/state.h>
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

static void close_socket(socket_t &socket) {
    if (socket == BAD_SOCK)
        return;

#ifdef _WIN32
    closesocket(socket);
#else
    // Closing alone doesn't wake a thread blocked in accept(), which would hang exit until a client connects
    shutdown(socket, SHUT_RDWR);
    close(socket);
#endif
    socket = BAD_SOCK;
}

static void close_gdb_sockets(GDBState &gdb) {
    close_socket(gdb.client_socket);
    close_socket(gdb.listen_socket);
}

static void cleanup_gdb_platform(GDBState &gdb) {
#ifdef _WIN32
    if (gdb.winsock_started) {
        WSACleanup();
        gdb.winsock_started = false;
    }
#endif
}

static void join_server_thread(GDBState &gdb) {
    std::shared_ptr<std::thread> server_thread;
    {
        const auto guard = std::lock_guard(gdb.server_thread_mutex);
        server_thread = std::move(gdb.server_thread);
    }

    if (!server_thread || !server_thread->joinable())
        return;

    if (server_thread->get_id() == std::this_thread::get_id()) {
        server_thread->detach();
    } else {
        server_thread->join();
    }
}

GDBState::~GDBState() {
    server_die = true;
    close_gdb_sockets(*this);
    join_server_thread(*this);
    cleanup_gdb_platform(*this);
}

// Must match the PacketSize advertised in qSupported.
typedef char PacketData[16384];

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
    return "PacketSize=4000;"
           "multiprocess-;swbreak+;hwbreak-;qRelocInsn-;fork-events-;vfork-events-;"
           "exec-events-;vContSupported+;QThreadEvents-;no-resumed-;qXfer:features:read+;"
           "qXfer:libraries:read+;qXfer:libraries-svr4:read+";
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

// Shared chunking logic for qXfer:*:read packets. Validates the prefix,
// parses "<offset>,<length>", and returns the appropriate "m<chunk>"
// (more) / "l<chunk>" (last) / "l" (eof) reply.
static std::string xfer_chunk_reply(const std::string &content, const std::string &prefix, const std::string &xml) {
    if (content.substr(0, prefix.size()) != prefix)
        return "E00";

    const std::string params = content.substr(prefix.size());
    const size_t comma = params.find(',');
    const uint32_t offset = parse_hex(params.substr(0, comma));
    const uint32_t length = parse_hex(params.substr(comma + 1));

    if (offset >= xml.size())
        return "l";
    const std::string part = xml.substr(offset, length);
    return (offset + part.size() < xml.size() ? "m" : "l") + part;
}

static std::string cmd_target_xml(EmuEnvState &state, PacketCommand &command) {
    return xfer_chunk_reply(content_string(command), "qXfer:features:read:target.xml:", TARGET_XML);
}

// Convert a Vita path like "vs0:sys/external/libc.suprx" into a slash-only
// form like "/vs0/sys/external/libc.suprx". gdb's BFD layer parses the
// colon in "device:" as a filename/section separator and refuses to open
// such paths, so we hand it a colon-free synthetic path in the library
// list and translate back on the vFile side.
static std::string vita_path_to_gdb_path(const std::string &vita) {
    const size_t colon = vita.find(':');
    if (colon == std::string::npos)
        return vita;
    return "/" + vita.substr(0, colon) + "/" + vita.substr(colon + 1);
}

// Inverse of the above: accept either a real Vita path ("ux0:/foo") or
// the slash-only synthetic form ("/ux0/foo") and return what `open_file`
// expects. Pass-through if the input doesn't match the synthetic shape.
static std::string gdb_path_to_vita_path(const std::string &gdb_path) {
    if (gdb_path.empty() || gdb_path[0] != '/')
        return gdb_path;
    const size_t second_slash = gdb_path.find('/', 1);
    if (second_slash == std::string::npos)
        return gdb_path;
    return gdb_path.substr(1, second_slash - 1) + ":" + gdb_path.substr(second_slash + 1);
}

// Build the library list XML. Used by both the legacy `library-list` and
// the svr4 variant; gdb dispatches on which qXfer name it requested.
static std::string build_library_list_xml(EmuEnvState &state, bool svr4) {
    std::string xml;
    if (svr4)
        xml = "<library-list-svr4 version=\"1.0\">";
    else
        xml = "<library-list>";

    {
        const auto guard = std::lock_guard(state.kernel.mutex);
        // Skip the main module — qOffsets already tells gdb its load address.
        bool first = true;
        for (const auto &[uid, module] : state.kernel.loaded_modules) {
            if (first) {
                first = false;
                continue;
            }
            // A module from the boot image has no file of its own, and gdb stops loading
            // symbols with an error when it can't open one
            if (module->elf_bytes.empty())
                continue;
            // Prefer the real Vita-style path so gdb can vFile:open it and
            // auto-load the .suprx symbols. Fall back to module name for any
            // module loaded without a recorded path.
            std::string name = !module->vita_path.empty()
                ? vita_path_to_gdb_path(module->vita_path)
                : std::string(module->info.module_name);
            // Defensive: drop any embedded null bytes that would break XML.
            name.erase(std::remove(name.begin(), name.end(), '\0'), name.end());

            if (svr4) {
                const uint32_t base = module->info.segments[0].vaddr.address();
                xml += fmt::format(
                    "<library name=\"{}\" lm=\"0x{:x}\" l_addr=\"0x{:x}\" l_ld=\"0x0\"/>",
                    name, static_cast<uint32_t>(uid), base);
            } else {
                // Emit a <segment> per loadable PT_LOAD in ELF order so gdb
                // can compute the load offset for each section.
                // Without this, symbols past segment 0 land at
                // bogus addresses
                xml += fmt::format("<library name=\"{}\">", name);
                for (size_t i = 0; i < MODULE_INFO_NUM_SEGMENTS; i++) {
                    const auto &seg = module->info.segments[i];
                    if (seg.size == 0)
                        continue;
                    xml += fmt::format("<segment address=\"{:#x}\"/>", seg.vaddr.address());
                }
                xml += "</library>";
            }
        }
    }

    xml += svr4 ? "</library-list-svr4>" : "</library-list>";
    return xml;
}

static std::string cmd_xfer_libraries(EmuEnvState &state, PacketCommand &command) {
    return xfer_chunk_reply(content_string(command),
        "qXfer:libraries:read::", build_library_list_xml(state, false));
}

static std::string cmd_xfer_libraries_svr4(EmuEnvState &state, PacketCommand &command) {
    return xfer_chunk_reply(content_string(command),
        "qXfer:libraries-svr4:read::", build_library_list_xml(state, true));
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

    const CPUContext context = state.kernel.threads[state.gdb.current_thread]->cpu->save_context();

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

    const CPUContext context = state.kernel.threads[state.gdb.current_thread]->cpu->save_context();

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
    // The write may be to code the JIT already compiled
    state.kernel.invalidate_jit_cache(address, length);

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
    // The write may be to code the JIT already compiled
    state.kernel.invalidate_jit_cache(address, length);

    return "OK";
}

static void release_app(EmuEnvState &state) {
    state.kernel.debugger.remove_all_breakpoints(state.mem);
    state.kernel.set_threads_suspended(false);

    state.gdb.current_thread = 0;
    state.gdb.stop_after_frames = 0;
    {
        const std::lock_guard<std::mutex> guard(state.ctrl.mutex);
        state.ctrl.scripted_input = {};
        state.ctrl.scripted_touch = {};
    }
    state.audio.switch_state(false);
}

static std::string cmd_detach(EmuEnvState &state, PacketCommand &command) {
    release_app(state);
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

// The thread to report for a stop no thread asked for. While module_start runs, the main thread
// exists but hasn't started, and gdb can't step it.
static SceUID started_thread(EmuEnvState &state) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    const auto started = [](const ThreadStatePtr &thread) {
        const ThreadStatus status = thread->get_status();
        return status != ThreadStatus::dormant && status != ThreadStatus::deleted;
    };
    const auto &threads = state.kernel.threads;
    if (const auto it = threads.find(state.gdb.current_thread); it != threads.end() && started(it->second))
        return it->first;
    SceUID chosen = 0;
    for (const auto &[uid, thread] : threads) {
        if (!started(thread))
            continue;
        // The main thread has the title ID for a name
        if (thread->name == state.io.title_id)
            return uid;
        if (chosen == 0)
            chosen = uid;
    }
    if (chosen == 0 && !threads.empty())
        chosen = threads.begin()->first;
    return chosen;
}

static std::vector<ThreadStatePtr> list_threads(EmuEnvState &state) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    std::vector<ThreadStatePtr> threads;
    for (const auto &[_, thread] : state.kernel.threads)
        threads.push_back(thread);
    return threads;
}

// A thread inside a kernel call stops when the call returns, which can take until the next continue
// if the call waits on another thread, like sceGxmDisplayQueueAddEntry does, so the wait is limited.
// Such a thread runs no guest code.
static void stop_app(EmuEnvState &state) {
    state.kernel.set_threads_suspended(true);
    const auto stop_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    for (const ThreadStatePtr &thread : list_threads(state))
        thread->wait_until_not_running(stop_deadline);
    state.audio.switch_state(true);
}

static std::string cmd_continue(EmuEnvState &state, PacketCommand &command) {
    const std::string content = content_string(command);

    // Build a T05 stop reply. If a watchpoint triggered, include the
    // watch address and type so gdb displays "Hardware watchpoint N"
    // instead of a generic SIGTRAP.
    const auto stop_reply = [&](SceUID tid) -> std::string {
        Address wp = state.kernel.debugger.break_watch_addr;
        if (wp) {
            state.kernel.debugger.break_watch_addr = 0;
            const char *keyword = "watch";
            switch (state.kernel.debugger.break_watch_type) {
            case WatchType::READ:
                keyword = "rwatch";
                break;
            case WatchType::ACCESS:
                keyword = "awatch";
                break;
            default:
                break;
            }
            return fmt::format("T05{}:{:x};thread:{:x};", keyword, wp, tid);
        }
        return fmt::format("T05thread:{:x};", tid);
    };

    uint64_t index = 5;
    uint64_t next = 0;
    do {
        next = content.find(';', index + 1);
        std::string text = content.substr(index + 1, next - index - 1);

        const char cmd = text[0];

        // Poll the client socket (non-blocking) for a Ctrl-C interrupt
        // byte. Returns true if one was consumed. Keeps long range
        // steps interruptible.
        const auto poll_ctrl_c = [&]() {
            fd_set read_set;
            timeval tv = { 0, 0 };
            FD_ZERO(&read_set);
            FD_SET(state.gdb.client_socket, &read_set);
            if (select(state.gdb.client_socket + 1, &read_set, nullptr, nullptr, &tv) <= 0)
                return false;
            char byte = 0;
            if (recv(state.gdb.client_socket, &byte, 1, MSG_PEEK) == 1 && byte == '\x03') {
                recv(state.gdb.client_socket, &byte, 1, 0);
                return true;
            }
            return false;
        };

        switch (cmd) {
        case 'r': {
            // Server-side range stepping: single-step the thread until PC
            // leaves [start, end). gdb would otherwise send one vCont;r per
            // instruction (a round trip per insn for every source-line
            // `next`), which is painfully slow over even a local socket.
            const size_t comma = text.find(',');
            if (comma == std::string::npos) {
                LOG_WARN("Malformed vCont;r packet: {}", text);
                break;
            }
            const size_t thread_delim = text.find(':', comma + 1);
            const uint32_t range_start = parse_hex(text.substr(1, comma - 1));
            const size_t end_len = thread_delim == std::string::npos
                ? std::string::npos
                : thread_delim - comma - 1;
            const uint32_t range_end = parse_hex(text.substr(comma + 1, end_len));

            // The action names the thread to step, else it's the current one
            SceUID thread_id = thread_delim == std::string::npos ? 0 : static_cast<SceUID>(parse_hex(text.substr(thread_delim + 1)));
            if (thread_id <= 0)
                thread_id = state.gdb.current_thread;
            const ThreadStatePtr thread = state.kernel.get_thread(thread_id);
            if (!thread)
                return "E00";

            // Poll the socket periodically so the user can still Ctrl-C
            // out of a pathological in-range loop.
            constexpr int CTRLC_POLL_INTERVAL = 1024;
            int steps = 0;
            while (!state.gdb.server_die) {
                const uint32_t pc = thread->cpu->get_pc();
                if (pc < range_start || pc >= range_end)
                    break;

                // A thread something else holds can't step, and gdb takes the error as a stop
                if (!thread->step())
                    return "E01";
                ++steps;

                const ThreadStatus status = thread->get_status();
                if (status == ThreadStatus::dormant || status == ThreadStatus::deleted)
                    break;
                if (state.kernel.debugger.has_break.exchange(false))
                    break;
                if ((steps % CTRLC_POLL_INTERVAL) == 0 && poll_ctrl_c()) {
                    LOG_INFO("GDB interrupt received during range step (Ctrl-C)");
                    break;
                }
            }

            if (state.gdb.server_die)
                return "";

            state.gdb.current_thread = thread_id;
            return stop_reply(thread_id);
        }
        case 's':
        case 'S': {
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
            uint64_t stop_vcount = 0;
            if (state.gdb.stop_after_frames != 0) {
                stop_vcount = state.display.vblank_count.load() + state.gdb.stop_after_frames;
                state.gdb.stop_after_frames = 0;
            }

            // Resume the world. A thread something else holds stays stopped.
            state.kernel.debugger.has_break = false;
            state.kernel.debugger.break_watch_addr = 0;
            state.kernel.set_threads_suspended(false);
            state.audio.switch_state(false);

            // Wait for a breakpoint hit (condvar) or a Ctrl-C interrupt
            // (\x03 byte on the socket). The condvar fires instantly on
            // breakpoint; the timeout is only for the frame limit and the interrupt poll.
            const auto poll_interval = std::chrono::milliseconds(stop_vcount != 0 ? 5 : 100);
            SceUID stopped_id = 0;
            bool did_break = false;
            bool interrupted = false;
            while (!did_break && !interrupted && !state.gdb.server_die) {
                {
                    std::unique_lock<std::mutex> bk_lock(state.kernel.debugger.break_mutex);
                    state.kernel.debugger.break_cv.wait_for(bk_lock, poll_interval, [&]() {
                        return state.kernel.debugger.has_break.load() || state.gdb.server_die;
                    });
                }

                if (state.kernel.debugger.has_break.load()) {
                    stopped_id = state.kernel.debugger.break_thread_id;
                    state.kernel.debugger.has_break = false;
                    did_break = true;
                }

                if (!did_break && stop_vcount != 0 && state.display.vblank_count.load() >= stop_vcount) {
                    interrupted = true;
                    stopped_id = started_thread(state);
                }

                // Check for gdb interrupt (Ctrl-C sends raw \x03).
                if (!did_break && !interrupted) {
                    fd_set read_set;
                    timeval tv = { 0, 0 };
                    FD_ZERO(&read_set);
                    FD_SET(state.gdb.client_socket, &read_set);
                    if (select(state.gdb.client_socket + 1, &read_set, nullptr, nullptr, &tv) > 0) {
                        char byte;
                        const auto peeked = recv(state.gdb.client_socket, &byte, 1, MSG_PEEK);
                        if (peeked == 1 && byte == '\x03') {
                            recv(state.gdb.client_socket, &byte, 1, 0);
                            interrupted = true;
                            LOG_INFO("GDB interrupt received (Ctrl-C)");
                            stopped_id = started_thread(state);
                        } else if (peeked <= 0) {
                            // The client is gone. Stop like an interrupt; the next read sees the
                            // closed connection and resumes the app.
                            interrupted = true;
                            LOG_INFO("GDB client disconnected during continue");
                            stopped_id = started_thread(state);
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

            stop_app(state);

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

static std::string cmd_reason(EmuEnvState &state, PacketCommand &command) {
    const SceUID chosen = started_thread(state);
    if (chosen == 0)
        return "S05";
    state.gdb.current_thread = chosen;
    return fmt::format("T05thread:{:x};", chosen);
}

static std::string cmd_get_first_thread(EmuEnvState &state, PacketCommand &command) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    state.gdb.thread_info_index = 0;

    if (state.kernel.threads.empty())
        return "l";

    return 'm' + to_hex(state.kernel.threads.begin()->first);
}

static std::string cmd_get_next_thread(EmuEnvState &state, PacketCommand &command) {
    const auto guard = std::lock_guard(state.kernel.mutex);
    std::string str;

    // Threads can exit between the packets, leaving the index past the end
    ++state.gdb.thread_info_index;
    if (state.gdb.thread_info_index >= state.kernel.threads.size()) {
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
    const uint32_t address = parse_hex(content.substr(first + 1, second - 1 - first));
    const uint32_t kind = static_cast<uint32_t>(std::stol(content.substr(second + 1, content.size() - second - 1)));

    if (type == 0) {
        const uint32_t bp_size = (kind == 2 || kind == 3) ? 2 : 4;
        if (!check_memory_region(address, bp_size, state.mem)) {
            LOG_WARN("GDB Server Breakpoint rejected — address {} not mapped.", log_hex(address));
            return "E01";
        }
        LOG_INFO("GDB Server New Breakpoint at {} ({}, {}).", log_hex(address), type, kind);
        // GDB sets bit 0 of the address for Thumb breakpoints; the real
        // instruction is 2-byte aligned. Keep the raw address for the
        // mapping check and log, but strip the Thumb bit before recording.
        // kind is 2 for a 16-bit Thumb instruction, 3 for a 32-bit one and 4 for ARM. A 16-bit BKPT
        // over the first halfword stops at a 32-bit Thumb instruction too.
        // https://sourceware.org/gdb/current/onlinedocs/gdb/ARM-Breakpoint-Kinds.html#ARM-Breakpoint-Kinds
        state.kernel.debugger.add_breakpoint(state.mem, align_down(address, 2), kind == 2 || kind == 3);
        return "OK";
    }

    if (type >= 2 && type <= 4) {
        LOG_INFO("GDB Server New Watchpoint at {} (type={}, len={}).", log_hex(address), type, kind);
        state.kernel.debugger.add_watch_memory_addr(address, kind, static_cast<WatchType>(type));
        state.kernel.debugger.watch_memory = true;
        state.kernel.debugger.update_watches();
        return "OK";
    }

    return "";
}

static std::string cmd_remove_breakpoint(EmuEnvState &state, PacketCommand &command) {
    const std::string content = content_string(command);

    const uint64_t first = content.find(',');
    const uint64_t second = content.find(',', first + 1);
    const uint32_t type = static_cast<uint32_t>(std::stol(content.substr(1, first - 1)));
    const uint32_t address = parse_hex(content.substr(first + 1, second - 1 - first));
    const uint32_t kind = static_cast<uint32_t>(std::stol(content.substr(second + 1, content.size() - second - 1)));

    if (type == 0) {
        LOG_INFO("GDB Server Removed Breakpoint at {} ({}, {}).", log_hex(address), type, kind);
        // Mirror cmd_add_breakpoint: strip the GDB Thumb-state bit so the
        // lookup key matches the address used at insertion.
        state.kernel.debugger.remove_breakpoint(state.mem, align_down(address, 2));
        return "OK";
    }

    if (type >= 2 && type <= 4) {
        LOG_INFO("GDB Server Removed Watchpoint at {} (type={}).", log_hex(address), type);
        state.kernel.debugger.remove_watch_memory_addr(state.kernel, address);
        return "OK";
    }

    return "";
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
        const std::string raw_path = vfile_unhex_path(args.substr(0, c1));
        // gdb's BFD layer can't tolerate ':' in remote filenames, so the
        // library list emits "/vs0/.../foo.suprx" instead of "vs0:.../foo
        // .suprx". Translate back to the device-prefix form open_file wants.
        const std::string path = gdb_path_to_vita_path(raw_path);
        const int gdb_flags = static_cast<int>(std::stoul(args.substr(c1 + 1, c2 - c1 - 1), nullptr, 16));
        const int sce_flags = gdb_flags_to_sce(gdb_flags);

        // Step 1: try the loaded-module ELF cache. Only modules loaded
        // with gdbstub on populate elf_bytes, and only those produce
        // BFD-parseable ELFs (rather than encrypted SELF blobs).
        std::vector<uint8_t> buffer;
        bool served_from_module = false;
        {
            const auto guard = std::lock_guard(state.kernel.mutex);
            for (const auto &[uid, module] : state.kernel.loaded_modules) {
                if (!module->elf_bytes.empty() && module->vita_path == path) {
                    buffer = module->elf_bytes;
                    served_from_module = true;
                    break;
                }
            }
        }

        // Step 2: fall back to reading the raw file from the guest fs
        // (lets `remote get` and other arbitrary fetches still work).
        if (!served_from_module) {
            const SceUID rfd = open_file(state.io, path.c_str(), sce_flags, state.vita_fs_path, "gdbstub");
            if (rfd < 0) {
                LOG_INFO("GDB vFile open failed: '{}' (raw='{}')", path, raw_path);
                return vfile_error(GDB_ENOENT);
            }
            // Slurp the whole file. seek to end to find size, then read.
            const SceOff end = seek_file(rfd, 0, SCE_SEEK_END, state.io, "gdbstub");
            seek_file(rfd, 0, SCE_SEEK_SET, state.io, "gdbstub");
            if (end > 0) {
                buffer.resize(static_cast<size_t>(end));
                const int got = read_file(buffer.data(), state.io, rfd, static_cast<SceSize>(end), "gdbstub");
                if (got < 0) {
                    close_file(state.io, rfd, "gdbstub");
                    return vfile_error(GDB_EBADF);
                }
                buffer.resize(static_cast<size_t>(got));
            }
            close_file(state.io, rfd, "gdbstub");
        }

        const int fd = state.gdb.next_vfile_fd++;
        state.gdb.vfile_buffers[fd] = std::move(buffer);
        LOG_INFO("GDB vFile opened: '{}' fd={} ({} bytes, {})", path, fd,
            state.gdb.vfile_buffers[fd].size(),
            served_from_module ? "decrypted module ELF" : "raw file");
        return fmt::format("F{:x}", fd);
    }

    // close:<fd-hex>
    if (sub.compare(0, 6, "close:") == 0) {
        const int fd = static_cast<int>(std::stoul(sub.substr(6), nullptr, 16));
        if (state.gdb.vfile_buffers.erase(fd) == 0)
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
        const int fd = static_cast<int>(std::stoul(args.substr(0, c1), nullptr, 16));
        uint32_t count = static_cast<uint32_t>(std::stoul(args.substr(c1 + 1, c2 - c1 - 1), nullptr, 16));
        const uint64_t offset = std::stoull(args.substr(c2 + 1), nullptr, 16);

        const auto it = state.gdb.vfile_buffers.find(fd);
        if (it == state.gdb.vfile_buffers.end())
            return vfile_error(GDB_EBADF);
        const std::vector<uint8_t> &buf = it->second;

        // Cap to fit in PacketData (16384) with reply header + worst-case
        // 2x binary escape expansion.
        constexpr uint32_t MAX_PREAD = 8000;
        if (count > MAX_PREAD)
            count = MAX_PREAD;

        if (offset >= buf.size())
            return "F0;"; // EOF
        const uint64_t avail = buf.size() - offset;
        const uint32_t n = count < avail ? count : static_cast<uint32_t>(avail);

        return fmt::format("F{:x};", n)
            + vfile_escape_binary(reinterpret_cast<const char *>(buf.data() + offset), n);
    }

    // fstat:<fd-hex>
    if (sub.compare(0, 6, "fstat:") == 0) {
        const int fd = static_cast<int>(std::stoul(sub.substr(6), nullptr, 16));
        const auto it = state.gdb.vfile_buffers.find(fd);
        if (it == state.gdb.vfile_buffers.end())
            return vfile_error(GDB_EBADF);
        const uint64_t size = it->second.size();

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
        vfile_put64_be(st + 28, size); // size
        vfile_put64_be(st + 36, 4096); // blksize
        vfile_put64_be(st + 44, (size + 511) / 512); // blocks
        vfile_put32_be(st + 52, 0); // atime
        vfile_put32_be(st + 56, 0); // mtime
        vfile_put32_be(st + 60, 0); // ctime

        return fmt::format("F{:x};", 64) + vfile_escape_binary(st, 64);
    }

    LOG_GDB_PACKET(state, "GDB Server: Unimplemented vFile op. {}", content);
    return "";
}

struct MonitorButton {
    std::string_view name;
    uint32_t buttons;
    uint32_t buttons_ext;
};

// sceCtrlRead and sceCtrlReadBufferPositive2 report the shoulder buttons differently
constexpr MonitorButton MONITOR_BUTTONS[] = {
    { "select", SCE_CTRL_SELECT, SCE_CTRL_SELECT },
    { "start", SCE_CTRL_START, SCE_CTRL_START },
    { "up", SCE_CTRL_UP, SCE_CTRL_UP },
    { "right", SCE_CTRL_RIGHT, SCE_CTRL_RIGHT },
    { "down", SCE_CTRL_DOWN, SCE_CTRL_DOWN },
    { "left", SCE_CTRL_LEFT, SCE_CTRL_LEFT },
    { "triangle", SCE_CTRL_TRIANGLE, SCE_CTRL_TRIANGLE },
    { "circle", SCE_CTRL_CIRCLE, SCE_CTRL_CIRCLE },
    { "cross", SCE_CTRL_CROSS, SCE_CTRL_CROSS },
    { "square", SCE_CTRL_SQUARE, SCE_CTRL_SQUARE },
    { "l1", SCE_CTRL_L, SCE_CTRL_L1 },
    { "r1", SCE_CTRL_R, SCE_CTRL_R1 },
    { "l2", 0, SCE_CTRL_L2 },
    { "r2", 0, SCE_CTRL_R2 },
    { "l3", 0, SCE_CTRL_L3 },
    { "r3", 0, SCE_CTRL_R3 },
};

constexpr uint32_t MONITOR_DEFAULT_FRAMES = 6;

static const char *const MONITOR_HELP = R"(screenshot <path>                  save the current frame as a PNG
press <button>[+<button>] [frames] press buttons, for 6 frames by default
hold <button>[+<button>]           press buttons until released
stick <left|right> <x> <y> [frames] move a stick, -1 to 1, until released by default
touch <x> <y> [frames]             touch the front screen at a point in 960x544, until released by default
release                            release all input
stop-after <frames>                stop the next continue after this many frames
Lengths start when the app reads the input. Buttons: select start up right down left triangle
circle cross square l1 r1 l2 r2 l3 r3)";

static bool parse_buttons(const std::string &names, uint32_t &buttons, uint32_t &buttons_ext) {
    std::istringstream stream(names);
    std::string name;
    while (std::getline(stream, name, '+')) {
        const auto button = std::find_if(std::begin(MONITOR_BUTTONS), std::end(MONITOR_BUTTONS), [&](const MonitorButton &entry) {
            return entry.name == name;
        });
        if (button == std::end(MONITOR_BUTTONS))
            return false;
        buttons |= button->buttons;
        buttons_ext |= button->buttons_ext;
    }
    return !names.empty();
}

static std::string run_monitor_command(EmuEnvState &state, const std::string &line) {
    std::istringstream args(line);
    std::string name;
    args >> name;

    // An optional trailing length. Fails on anything but a number or the end of the line.
    const auto read_frames = [&](uint32_t &frames) {
        args >> std::ws;
        return args.eof() || (args >> frames && !args.fail());
    };

    if (name.empty() || name == "help")
        return MONITOR_HELP;

    if (name == "screenshot") {
        std::string path;
        std::getline(args >> std::ws, path);
        if (path.empty())
            return "usage: screenshot <path>";
        if (!state.gdb.save_frame || !state.gdb.save_frame(path))
            return "error: no frame could be saved";
        return "saved " + path;
    }

    if (name == "press" || name == "hold") {
        std::string names;
        args >> names;
        uint32_t buttons = 0;
        uint32_t buttons_ext = 0;
        uint32_t frames = name == "press" ? MONITOR_DEFAULT_FRAMES : 0;
        if (!parse_buttons(names, buttons, buttons_ext) || (name == "press" && !read_frames(frames)))
            return "error: bad arguments, see monitor help";
        const std::lock_guard<std::mutex> guard(state.ctrl.mutex);
        auto &scripted = state.ctrl.scripted_input;
        scripted.buttons |= buttons;
        scripted.buttons_ext |= buttons_ext;
        scripted.length = { .frames = frames };
        return "pressed " + names;
    }

    if (name == "stick") {
        std::string side;
        float x = 0;
        float y = 0;
        uint32_t frames = 0;
        args >> side >> x >> y;
        if ((side != "left" && side != "right") || args.fail() || !read_frames(frames))
            return "error: bad arguments, see monitor help";
        const int axis = side == "left" ? 0 : 2;
        const std::lock_guard<std::mutex> guard(state.ctrl.mutex);
        auto &scripted = state.ctrl.scripted_input;
        scripted.axes[axis] = std::clamp(x, -1.f, 1.f);
        scripted.axes[axis + 1] = std::clamp(y, -1.f, 1.f);
        scripted.length = { .frames = frames };
        return "moved " + side + " stick";
    }

    if (name == "touch") {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t frames = 0;
        args >> x >> y;
        if (args.fail() || x >= 960 || y >= 544 || !read_frames(frames))
            return "error: bad arguments, see monitor help";
        const std::lock_guard<std::mutex> guard(state.ctrl.mutex);
        auto &scripted = state.ctrl.scripted_touch;
        scripted.active = true;
        scripted.x = static_cast<uint16_t>(x * 2);
        scripted.y = static_cast<uint16_t>(y * 2);
        scripted.length = { .frames = frames };
        return "touching";
    }

    if (name == "release") {
        const std::lock_guard<std::mutex> guard(state.ctrl.mutex);
        state.ctrl.scripted_input = {};
        state.ctrl.scripted_touch = {};
        return "released";
    }

    if (name == "stop-after") {
        uint32_t frames = 0;
        if (!(args >> frames))
            return "error: bad arguments, see monitor help";
        state.gdb.stop_after_frames = frames;
        return "stopping after " + std::to_string(frames) + " frames";
    }

    return "unknown command, see monitor help";
}

static std::string cmd_monitor(EmuEnvState &state, PacketCommand &command) {
    const std::string content = content_string(command);
    constexpr size_t PREFIX_LENGTH = sizeof("qRcmd,") - 1;

    std::string line;
    for (size_t i = PREFIX_LENGTH; i + 1 < content.size(); i += 2)
        line.push_back(static_cast<char>(parse_hex(content.substr(i, 2))));

    const std::string output = run_monitor_command(state, line) + "\n";
    std::string reply;
    for (const char c : output)
        fmt::format_to(std::back_inserter(reply), "{:02x}", static_cast<uint8_t>(c));
    return reply;
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
    { "qXfer:libraries-svr4:read:", cmd_xfer_libraries_svr4 },
    { "qXfer:libraries:read:", cmd_xfer_libraries },
    { "qfThreadInfo", cmd_get_first_thread },
    { "qsThreadInfo", cmd_get_next_thread },
    { "qSupported", cmd_supported },
    { "qXfer:features:read:target.xml:", cmd_target_xml },
    { "qOffsets", cmd_offsets },
    { "qAttached", cmd_attached },
    { "qTStatus", cmd_thread_status },
    { "qC", cmd_get_current_thread },
    { "qRcmd,", cmd_monitor },
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

        // The app runs until a client attaches, unless it waits for the debugger
        stop_app(state);

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
        state.gdb.vfile_buffers.clear();
        state.gdb.next_vfile_fd = 1;

        if (!state.gdb.server_die) {
            // A client that quits without detaching would otherwise leave the app stopped
            release_app(state);
            LOG_INFO("GDB Server Client Disconnected, waiting for new connection...");
        }
    }

    // The owning thread handles server_close(); doing it here would race
    // join/detach against process shutdown.
    close_socket(state.gdb.listen_socket);
}

void server_open(EmuEnvState &state) {
    if (state.gdb.server_thread) {
        if (!state.gdb.server_die && state.gdb.listen_socket != BAD_SOCK) {
            LOG_INFO("GDB Server is already listening on port {}", GDB_SERVER_PORT);
            return;
        }

        join_server_thread(state.gdb);
    }

    state.gdb.server_die = false;

    LOG_INFO("Starting GDB Server...");

    // The threads may wait for the debugger's first continue, which can't come without a server
    const auto fail = [&](const char *reason) {
        LOG_ERROR("GDB Server Failed: {}", reason);
        close_socket(state.gdb.listen_socket);
        cleanup_gdb_platform(state.gdb);
        state.kernel.set_threads_suspended(false);
    };

#ifdef _WIN32
    int32_t err = WSAStartup(MAKEWORD(2, 2), &state.gdb.wsaData);
    if (err) {
        fail("Could not start WSA service.");
        return;
    }
    state.gdb.winsock_started = true;
#endif

    state.gdb.listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (state.gdb.listen_socket == -1) {
        fail("Could not create socket.");
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
        fail("Could not bind socket.");
        return;
    }

    if (listen(state.gdb.listen_socket, 1) == -1) {
        fail("Could not listen on socket.");
        return;
    }

    state.gdb.server_thread = std::make_shared<std::thread>(server_listen, std::ref(state));

    if (state.cfg.wait_for_debugger)
        LOG_WARN("GDB Server is listening on port {}. The app waits for a client to connect and continue.", GDB_SERVER_PORT);
    else
        LOG_INFO("GDB Server is listening on port {}", GDB_SERVER_PORT);
}

void server_close(EmuEnvState &state) {
    state.gdb.server_die = true;
    close_gdb_sockets(state.gdb);
    join_server_thread(state.gdb);
    cleanup_gdb_platform(state.gdb);
}
