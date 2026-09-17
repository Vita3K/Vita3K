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

#include <cheat/functions.h>

#include <mem/ptr.h>
#include <mem/state.h>
#include <util/log.h>

#include <algorithm>

namespace cheat {

namespace {

// Safety net so that a malformed database cannot make the vblank thread walk codes forever.
constexpr size_t max_executed_lines = 4096;
// The format allows at most five levels of indirection.
constexpr uint8_t max_pointer_level = 5;

uint32_t width_size(CodeWidth width) {
    switch (width) {
    case CodeWidth::bits8: return 1;
    case CodeWidth::bits16: return 2;
    default: return 4;
    }
}

uint32_t width_mask(CodeWidth width) {
    switch (width) {
    case CodeWidth::bits8: return 0xFFu;
    case CodeWidth::bits16: return 0xFFFFu;
    default: return 0xFFFFFFFFu;
    }
}

// `is_valid_addr_range()` takes an end that is one past the last byte of the access.
bool is_accessible(const MemState &mem, uint32_t address, uint32_t size) {
    return (address != 0) && is_valid_addr_range(mem, address, address + size);
}

// The guest pages a value spans are not necessarily contiguous in host memory, so go byte by byte.
bool read_bytes(const MemState &mem, uint32_t address, uint8_t *destination, uint32_t size) {
    if (!is_accessible(mem, address, size))
        return false;

    for (uint32_t i = 0; i < size; ++i)
        destination[i] = *Ptr<uint8_t>(address + i).get(mem);

    return true;
}

bool write_bytes(MemState &mem, uint32_t address, const uint8_t *source, uint32_t size) {
    if (!is_accessible(mem, address, size))
        return false;

    for (uint32_t i = 0; i < size; ++i)
        *Ptr<uint8_t>(address + i).get(mem) = source[i];

    return true;
}

bool read_value(const MemState &mem, uint32_t address, CodeWidth width, uint32_t &value) {
    uint8_t bytes[4] = {};
    const uint32_t size = width_size(width);
    if (!read_bytes(mem, address, bytes, size))
        return false;

    value = 0;
    for (uint32_t i = 0; i < size; ++i)
        value |= static_cast<uint32_t>(bytes[i]) << (i * 8);

    return true;
}

bool write_value(MemState &mem, uint32_t address, CodeWidth width, uint32_t value) {
    uint8_t bytes[4] = {};
    const uint32_t size = width_size(width);
    for (uint32_t i = 0; i < size; ++i)
        bytes[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);

    return write_bytes(mem, address, bytes, size);
}

struct PointerBlock {
    uint32_t address = 0;
    std::vector<uint32_t> offsets;
    // Closing line, it carries the value for a pointer write and nothing for a pointer MOV.
    CodeLine closing;
};

class Interpreter {
public:
    Interpreter(const CheatState &state, Cheat &cheat, MemState &mem, uint32_t buttons, const JitInvalidate &invalidate_jit)
        : m_state(state)
        , m_cheat(cheat)
        , m_mem(mem)
        , m_buttons(buttons)
        , m_invalidate_jit(invalidate_jit) {}

    void run() {
        size_t executed = 0;
        while ((m_index < m_cheat.lines.size()) && (executed < max_executed_lines)) {
            ++executed;
            if (!execute(m_cheat.lines[m_index])) {
                m_cheat.broken = true;
                break;
            }
        }
    }

private:
    // Addresses become relative to a module segment once a `$B2` code has been seen.
    uint32_t resolve(uint32_t address) const {
        return m_relative_base + address;
    }

    size_t code_line_count(size_t index) const {
        if (index >= m_cheat.lines.size())
            return 0;

        const CodeLine &line = m_cheat.lines[index];
        switch (line.type()) {
        case CodeType::compression:
            return 2;
        case CodeType::pointer_write:
            return 1 + pointer_block_lines(line);
        case CodeType::pointer_compression:
            // The block, then the line holding the count and the gaps.
            return 1 + pointer_block_lines(line) + 1;
        case CodeType::pointer_mov: {
            // A pointer MOV is a destination block followed by a source block.
            const size_t destination = 1 + pointer_block_lines(line);
            const size_t source_index = index + destination;
            if ((source_index >= m_cheat.lines.size()) || (m_cheat.lines[source_index].type() != CodeType::pointer_mov))
                return destination;
            return destination + 1 + pointer_block_lines(m_cheat.lines[source_index]);
        }
        default:
            return 1;
        }
    }

    static size_t pointer_block_lines(const CodeLine &head) {
        return std::min<size_t>(head.param(), max_pointer_level);
    }

    // A related count is a number of codes, not of lines, and a pointer write spans several.
    void skip_related_codes(uint8_t related) {
        size_t remaining = std::max<size_t>(related, 1);
        while ((remaining > 0) && (m_index < m_cheat.lines.size())) {
            m_index += std::max<size_t>(code_line_count(m_index), 1);
            --remaining;
        }
    }

    bool execute(const CodeLine &line) {
        switch (line.type()) {
        case CodeType::write:
            return execute_write(line);
        case CodeType::pointer_write:
            return execute_pointer_write(line);
        case CodeType::compression:
            return execute_compression(line);
        case CodeType::mov:
            return execute_mov(line);
        case CodeType::pointer_compression:
            return execute_pointer_compression(line);
        case CodeType::pointer_mov:
            return execute_pointer_mov(line);
        case CodeType::arm_write:
            return execute_arm_write(line);
        case CodeType::relative_base:
            return execute_relative_base(line);
        case CodeType::button_pad:
            return execute_button_pad(line);
        case CodeType::condition:
            return execute_condition(line);
        default:
            LOG_WARN_ONCE("Cheat '{}' uses the unsupported code type ${:04X}", m_cheat.name, line.control);
            return false;
        }
    }

    // `$0X00 <address> <value>`
    bool execute_write(const CodeLine &line) {
        ++m_index;
        write_value(m_mem, resolve(line.first), static_cast<CodeWidth>(line.op()), line.second);
        return true;
    }

    // `$5X00 <destination> <source>`
    bool execute_mov(const CodeLine &line) {
        ++m_index;

        const auto width = static_cast<CodeWidth>(line.op());
        uint32_t value = 0;
        if (read_value(m_mem, resolve(line.second), width, value))
            write_value(m_mem, resolve(line.first), width, value);

        return true;
    }

    // `$4X01 <address> <value>` followed by `$<count> <address gap> <value gap>`
    bool execute_compression(const CodeLine &line) {
        if (m_index + 1 >= m_cheat.lines.size()) {
            LOG_WARN_ONCE("Cheat '{}' ends with an incomplete compression code", m_cheat.name);
            return false;
        }

        const CodeLine gaps = m_cheat.lines[m_index + 1];
        m_index += 2;

        const auto width = static_cast<CodeWidth>(line.op());
        // The whole control word of the second line is the iteration count.
        const uint32_t count = gaps.control;
        for (uint32_t i = 0; i < count; ++i)
            write_value(m_mem, resolve(line.first) + (i * gaps.first), width, line.second + (i * gaps.second));

        return true;
    }

    // `$3X<level> <address> <offset>`, one line per further offset, then the value.
    bool execute_pointer_write(const CodeLine &line) {
        PointerBlock block;
        if (!parse_pointer_block(line, block))
            return false;

        uint32_t address = 0;
        if (follow_pointers(block, address))
            write_value(m_mem, address, static_cast<CodeWidth>(line.op()), block.closing.second);

        return true;
    }

    // `$8X<level> ...` writes to `$8<4|5|6><level> ...`, both blocks closed by a `$88` / `$89` line.
    bool execute_pointer_mov(const CodeLine &line) {
        PointerBlock destination;
        if (!parse_pointer_block(line, destination))
            return false;

        if (m_index >= m_cheat.lines.size()) {
            LOG_WARN_ONCE("Cheat '{}' ends with an incomplete pointer MOV code", m_cheat.name);
            return false;
        }

        const CodeLine source_line = m_cheat.lines[m_index];
        if ((source_line.type() != CodeType::pointer_mov) || (source_line.op() < 4) || (source_line.op() > 6)) {
            LOG_WARN_ONCE("Cheat '{}' has a pointer MOV code without a source block", m_cheat.name);
            return false;
        }

        PointerBlock source;
        if (!parse_pointer_block(source_line, source))
            return false;

        const auto width = static_cast<CodeWidth>(line.op());
        uint32_t destination_address = 0;
        uint32_t source_address = 0;
        if (!follow_pointers(destination, destination_address) || !follow_pointers(source, source_address))
            return true;

        uint32_t value = 0;
        if (read_value(m_mem, source_address, width, value))
            write_value(m_mem, destination_address, width, value);

        return true;
    }

    // A `$3` block followed by the `$<count> <address gap> <value gap>` line of a `$4` code.
    bool execute_pointer_compression(const CodeLine &line) {
        PointerBlock block;
        if (!parse_pointer_block(line, block))
            return false;

        if (m_index >= m_cheat.lines.size()) {
            LOG_WARN_ONCE("Cheat '{}' ends with an incomplete pointer compression code", m_cheat.name);
            return false;
        }

        const CodeLine gaps = m_cheat.lines[m_index];
        ++m_index;

        uint32_t address = 0;
        if (!follow_pointers(block, address))
            return true;

        const auto width = static_cast<CodeWidth>(line.op());
        const uint32_t count = gaps.control;
        for (uint32_t i = 0; i < count; ++i)
            write_value(m_mem, address + (i * gaps.first), width, block.closing.second + (i * gaps.second));

        return true;
    }

    // `$AX00 <address> <instruction>`, X is 1 for a 16-bit and 2 for a 32-bit instruction.
    bool execute_arm_write(const CodeLine &line) {
        ++m_index;

        const auto width = (line.op() == 1) ? CodeWidth::bits16 : CodeWidth::bits32;
        const uint32_t size = width_size(width);
        const uint32_t address = resolve(line.first);
        const uint32_t value = line.second & width_mask(width);

        uint32_t current = 0;
        if (!read_value(m_mem, address, width, current))
            return true;

        // Patching guest code once is enough, only pay for the recompiler flush when it changed.
        if (current == value)
            return true;

        const bool already_saved = std::any_of(m_cheat.saved_memory.begin(), m_cheat.saved_memory.end(),
            [address](const SavedMemory &saved) { return saved.address == address; });
        if (!already_saved) {
            SavedMemory saved;
            saved.address = address;
            saved.bytes.resize(size);
            if (!read_bytes(m_mem, address, saved.bytes.data(), size))
                return true;
            m_cheat.saved_memory.push_back(std::move(saved));
        }

        if (write_value(m_mem, address, width, value) && m_invalidate_jit)
            m_invalidate_jit(address, size);

        return true;
    }

    // `$B2<module> 0000000<segment> 00000000`
    bool execute_relative_base(const CodeLine &line) {
        ++m_index;

        if (line.op() != 2) {
            LOG_WARN_ONCE("Cheat '{}' uses the unsupported code type ${:04X}", m_cheat.name, line.control);
            return false;
        }

        const uint8_t module_index = line.param();
        const uint32_t segment_index = line.first & 0xF;
        if ((module_index >= m_state.modules.size()) || (segment_index >= 2)) {
            LOG_WARN_ONCE("Cheat '{}' refers to module {} segment {}, which is not loaded", m_cheat.name, module_index, segment_index);
            return false;
        }

        const auto &segment = m_state.modules[module_index].segments[segment_index];
        if (segment.size == 0) {
            LOG_WARN_ONCE("Cheat '{}' refers to module {} segment {}, which is empty", m_cheat.name, module_index, segment_index);
            return false;
        }

        m_relative_base = segment.address;

        return true;
    }

    // `$C2<lines> <pad type> <button mask>`
    bool execute_button_pad(const CodeLine &line) {
        ++m_index;

        // The pad type selects between the Vita pad and a DualShock, Vita3K only exposes one pad.
        const uint32_t mask = line.second;
        if ((m_buttons & mask) != mask)
            skip_related_codes(line.param());

        return true;
    }

    // `$DX<lines> <address> <value>`
    bool execute_condition(const CodeLine &line) {
        ++m_index;

        const uint8_t op = line.op();
        if (op > 0xB) {
            LOG_WARN_ONCE("Cheat '{}' uses the unsupported condition ${:04X}", m_cheat.name, line.control);
            return false;
        }

        const auto width = static_cast<CodeWidth>(op % 3);
        const uint32_t mask = width_mask(width);
        const uint32_t expected = line.second & mask;

        uint32_t value = 0;
        bool satisfied = read_value(m_mem, resolve(line.first), width, value);
        if (satisfied) {
            switch (op / 3) {
            case 0: satisfied = value == expected; break;
            case 1: satisfied = value != expected; break;
            case 2: satisfied = value > expected; break;
            default: satisfied = value < expected; break;
            }
        }

        if (!satisfied)
            skip_related_codes(line.param());

        return true;
    }

    // Databases word the control field of the follow-up lines freely, so only `second` is read.
    bool parse_pointer_block(const CodeLine &head, PointerBlock &block) {
        const uint8_t level = head.param();
        if ((level == 0) || (level > max_pointer_level)) {
            LOG_WARN_ONCE("Cheat '{}' uses the invalid pointer level {} of code ${:04X}", m_cheat.name, level, head.control);
            return false;
        }

        if (m_index + level >= m_cheat.lines.size()) {
            LOG_WARN_ONCE("Cheat '{}' ends with an incomplete pointer block", m_cheat.name);
            return false;
        }

        block.address = head.first;
        block.offsets.reserve(level);
        block.offsets.push_back(head.second);
        for (uint8_t i = 1; i < level; i++)
            block.offsets.push_back(m_cheat.lines[m_index + i].second);

        block.closing = m_cheat.lines[m_index + level];
        m_index += level + 1;

        return true;
    }

    bool follow_pointers(const PointerBlock &block, uint32_t &address) const {
        address = resolve(block.address);
        for (const uint32_t offset : block.offsets) {
            uint32_t pointer = 0;
            if (!read_value(m_mem, address, CodeWidth::bits32, pointer))
                return false;
            address = pointer + offset;
        }

        return true;
    }

    const CheatState &m_state;
    Cheat &m_cheat;
    MemState &m_mem;
    const uint32_t m_buttons;
    const JitInvalidate &m_invalidate_jit;

    size_t m_index = 0;
    uint32_t m_relative_base = 0;
};

void restore_cheat(Cheat &cheat, MemState &mem, const JitInvalidate &invalidate_jit) {
    for (auto it = cheat.saved_memory.rbegin(); it != cheat.saved_memory.rend(); ++it) {
        const uint32_t size = static_cast<uint32_t>(it->bytes.size());
        if (write_bytes(mem, it->address, it->bytes.data(), size) && invalidate_jit)
            invalidate_jit(it->address, size);
    }

    cheat.saved_memory.clear();
}

} // namespace

void unload(CheatState &state) {
    const std::lock_guard<std::mutex> lock(state.mutex);

    state.file = {};
    state.modules.clear();
    state.buttons.store(0, std::memory_order_relaxed);
}

bool load(CheatState &state, const fs::path &cheats_dir, const std::string &title_id) {
    const auto path = find_cheat_file(cheats_dir, title_id);
    if (path.empty()) {
        LOG_DEBUG("No cheat file found for {} in {}", title_id, cheats_dir);
        return false;
    }

    CheatFile file = parse_cheat_file(path, title_id);
    for (auto &cheat : file.cheats)
        cheat.enabled = cheat.enabled_on_boot;

    const std::lock_guard<std::mutex> lock(state.mutex);
    state.file = std::move(file);

    return !state.file.cheats.empty();
}

bool reload(CheatState &state, const fs::path &cheats_dir, const std::string &title_id, MemState &mem, const JitInvalidate &invalidate_jit) {
    // The saved originals go away with the old cheats, so undo the ARM writes first.
    set_all_cheats_enabled(state, false, mem, invalidate_jit);

    return load(state, cheats_dir, title_id);
}

void add_module(CheatState &state, const CheatModule &module) {
    const std::lock_guard<std::mutex> lock(state.mutex);

    state.modules.push_back(module);
}

void set_buttons(CheatState &state, uint32_t buttons) {
    state.buttons.store(buttons, std::memory_order_relaxed);
}

void apply(CheatState &state, MemState &mem, const JitInvalidate &invalidate_jit) {
    const std::lock_guard<std::mutex> lock(state.mutex);

    const uint32_t buttons = state.buttons.load(std::memory_order_relaxed);

    for (auto &cheat : state.file.cheats) {
        if (state.enabled && cheat.enabled && !cheat.broken) {
            Interpreter(state, cheat, mem, buttons, invalidate_jit).run();
        } else if (!cheat.saved_memory.empty()) {
            restore_cheat(cheat, mem, invalidate_jit);
        }
    }
}

void set_cheat_enabled(CheatState &state, size_t index, bool enabled, MemState &mem, const JitInvalidate &invalidate_jit) {
    const std::lock_guard<std::mutex> lock(state.mutex);

    if (index >= state.file.cheats.size())
        return;

    Cheat &cheat = state.file.cheats[index];
    if (cheat.enabled == enabled)
        return;

    cheat.enabled = enabled;
    if (!enabled)
        restore_cheat(cheat, mem, invalidate_jit);
}

void set_all_cheats_enabled(CheatState &state, bool enabled, MemState &mem, const JitInvalidate &invalidate_jit) {
    const std::lock_guard<std::mutex> lock(state.mutex);

    for (auto &cheat : state.file.cheats) {
        cheat.enabled = enabled;
        if (!enabled)
            restore_cheat(cheat, mem, invalidate_jit);
    }
}

void set_enabled(CheatState &state, bool enabled, MemState &mem, const JitInvalidate &invalidate_jit) {
    const std::lock_guard<std::mutex> lock(state.mutex);

    if (state.enabled == enabled)
        return;

    state.enabled = enabled;
    if (!enabled) {
        for (auto &cheat : state.file.cheats)
            restore_cheat(cheat, mem, invalidate_jit);
    }
}

size_t enabled_cheat_count(const CheatState &state) {
    const std::lock_guard<std::mutex> lock(state.mutex);

    return std::count_if(state.file.cheats.begin(), state.file.cheats.end(),
        [](const Cheat &cheat) { return cheat.enabled; });
}

CheatFile snapshot(const CheatState &state) {
    const std::lock_guard<std::mutex> lock(state.mutex);

    return state.file;
}

bool save(CheatState &state) {
    const std::lock_guard<std::mutex> lock(state.mutex);

    if (!save_cheat_file(state.file))
        return false;

    for (auto &cheat : state.file.cheats)
        cheat.enabled_on_boot = cheat.enabled;

    return true;
}

} // namespace cheat
