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
#include <mutex>

namespace cheat {

namespace {

// Bounds the lines the vblank thread walks for each cheat every frame.
constexpr size_t max_lines = 4096;
// A `$4` or `$7` code repeats its write up to 65535 times, this bounds all of them in one cheat.
constexpr size_t max_repeated_writes = 0x10000;
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
bool read_bytes(MemState &mem, uint32_t address, uint8_t *destination, uint32_t size) {
    const std::lock_guard<std::mutex> lock(mem.generation_mutex);
    if (!is_accessible(mem, address, size))
        return false;

    for (uint32_t i = 0; i < size; ++i)
        destination[i] = *Ptr<uint8_t>(address + i).get(mem);

    return true;
}

bool write_bytes(MemState &mem, uint32_t address, const uint8_t *source, uint32_t size) {
    const std::lock_guard<std::mutex> lock(mem.generation_mutex);
    if (!is_accessible(mem, address, size))
        return false;

    for (uint32_t i = 0; i < size; ++i)
        *Ptr<uint8_t>(address + i).get(mem) = source[i];

    return true;
}

bool read_value(MemState &mem, uint32_t address, CodeWidth width, uint32_t &value) {
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

size_t pointer_block_lines(const CodeLine &head) {
    return std::min<size_t>(head.param(), max_pointer_level);
}

size_t code_line_count(const std::vector<CodeLine> &lines, size_t index) {
    if (index >= lines.size())
        return 0;

    const CodeLine &line = lines[index];
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
        if ((source_index >= lines.size()) || (lines[source_index].type() != CodeType::pointer_mov))
            return destination;
        return destination + 1 + pointer_block_lines(lines[source_index]);
    }
    default:
        return 1;
    }
}

// Runs once per cheat when it is loaded, the interpreter then trusts every code to be complete.
class Validator {
public:
    Validator(const Cheat &cheat, const std::vector<CheatModule> &modules)
        : m_cheat(cheat)
        , m_modules(modules) {}

    bool validate() const {
        if (m_cheat.lines.size() > max_lines) {
            LOG_WARN("Cheat '{}' has more than {} lines", m_cheat.name, max_lines);
            return false;
        }

        size_t index = 0;
        size_t repeated_writes = 0;
        while (index < m_cheat.lines.size()) {
            if (!validate_code(index))
                return false;
            repeated_writes += repeat_count(index);
            index += code_line_count(m_cheat.lines, index);
        }

        if (repeated_writes > max_repeated_writes) {
            LOG_WARN("Cheat '{}' repeats its writes more than {} times", m_cheat.name, max_repeated_writes);
            return false;
        }

        return true;
    }

private:
    // The count is the whole control word of the line that follows the code.
    size_t repeat_count(size_t index) const {
        const CodeLine &line = m_cheat.lines[index];
        if (line.type() == CodeType::compression)
            return m_cheat.lines[index + 1].control;
        if (line.type() == CodeType::pointer_compression)
            return m_cheat.lines[index + 1 + pointer_block_lines(line)].control;
        return 0;
    }

    bool validate_width(const CodeLine &line) const {
        if (line.op() <= static_cast<uint8_t>(CodeWidth::bits32))
            return true;

        LOG_WARN("Cheat '{}' uses the invalid width of code ${:04X}", m_cheat.name, line.control);
        return false;
    }

    bool validate_pointer_level(const CodeLine &line) const {
        const uint8_t level = line.param();
        if ((level > 0) && (level <= max_pointer_level))
            return true;

        LOG_WARN("Cheat '{}' uses the invalid pointer level {} of code ${:04X}", m_cheat.name, level, line.control);
        return false;
    }

    bool validate_length(size_t index) const {
        if (index + code_line_count(m_cheat.lines, index) <= m_cheat.lines.size())
            return true;

        LOG_WARN("Cheat '{}' ends with an incomplete code ${:04X}", m_cheat.name, m_cheat.lines[index].control);
        return false;
    }

    bool validate_code(size_t index) const {
        const CodeLine &line = m_cheat.lines[index];
        switch (line.type()) {
        case CodeType::write:
        case CodeType::mov:
            return validate_width(line);
        case CodeType::compression:
            return validate_width(line) && validate_length(index);
        case CodeType::pointer_write:
        case CodeType::pointer_compression:
            return validate_width(line) && validate_pointer_level(line) && validate_length(index);
        case CodeType::pointer_mov: {
            if (!validate_width(line) || !validate_pointer_level(line))
                return false;

            const size_t source_index = index + 1 + line.param();
            const CodeLine *source = (source_index < m_cheat.lines.size()) ? &m_cheat.lines[source_index] : nullptr;
            if (!source || (source->type() != CodeType::pointer_mov) || (source->op() < 4) || (source->op() > 6)) {
                LOG_WARN("Cheat '{}' has a pointer MOV code without a source block", m_cheat.name);
                return false;
            }

            return validate_pointer_level(*source) && validate_length(index);
        }
        case CodeType::arm_write:
            if ((line.op() == 1) || (line.op() == 2))
                return true;
            LOG_WARN("Cheat '{}' uses the invalid instruction size of code ${:04X}", m_cheat.name, line.control);
            return false;
        case CodeType::relative_base:
            return validate_relative_base(line);
        case CodeType::button_pad:
            return true;
        case CodeType::condition:
            if (line.op() <= 0xB)
                return true;
            LOG_WARN("Cheat '{}' uses the unsupported condition ${:04X}", m_cheat.name, line.control);
            return false;
        default:
            LOG_WARN("Cheat '{}' uses the unsupported code type ${:04X}", m_cheat.name, line.control);
            return false;
        }
    }

    // `$B2<module> 0000000<segment> 00000000`
    bool validate_relative_base(const CodeLine &line) const {
        if (line.op() != 2) {
            LOG_WARN("Cheat '{}' uses the unsupported code type ${:04X}", m_cheat.name, line.control);
            return false;
        }

        const uint8_t module_index = line.param();
        const uint32_t segment_index = line.first & 0xF;
        // How VitaCheat numbers the modules after the main executable is undocumented, so guessing could patch the wrong one.
        if ((module_index != 0) || m_modules.empty()) {
            LOG_WARN("Cheat '{}' refers to module {}, only module 0 (the main executable) is supported", m_cheat.name, module_index);
            return false;
        }

        const CheatModule &module = m_modules[module_index];
        if (segment_index >= module.segments.size()) {
            LOG_WARN("Cheat '{}' refers to segment {}, which does not exist", m_cheat.name, segment_index);
            return false;
        }
        if (module.segments[segment_index].size == 0) {
            LOG_WARN("Cheat '{}' refers to module {} segment {}, which is empty", m_cheat.name, module_index, segment_index);
            return false;
        }

        return true;
    }

    const Cheat &m_cheat;
    const std::vector<CheatModule> &m_modules;
};

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
        while (m_index < m_cheat.lines.size())
            execute(m_cheat.lines[m_index]);
    }

private:
    // Addresses become relative to a module segment once a `$B2` code has been seen.
    uint32_t resolve(uint32_t address) const {
        return m_relative_base + address;
    }

    // The byte at `address` before any ARM write patched it, whatever the size of that write or its cheat.
    uint8_t original_byte(uint32_t address, uint8_t current) const {
        for (const Cheat &cheat : m_state.file.cheats) {
            for (const SavedMemory &saved : cheat.saved_memory) {
                if ((address >= saved.address) && ((address - saved.address) < saved.bytes.size()))
                    return saved.bytes[address - saved.address];
            }
        }

        return current;
    }

    // A related count is a number of codes, not of lines, and a pointer write spans several.
    void skip_related_codes(uint8_t related) {
        size_t remaining = std::max<size_t>(related, 1);
        while ((remaining > 0) && (m_index < m_cheat.lines.size())) {
            // The base is a directive for what follows, the codes it precedes must still find it set.
            if (m_cheat.lines[m_index].type() == CodeType::relative_base) {
                execute_relative_base(m_cheat.lines[m_index]);
                continue;
            }

            m_index += std::max<size_t>(code_line_count(m_cheat.lines, m_index), 1);
            --remaining;
        }
    }

    void execute(const CodeLine &line) {
        switch (line.type()) {
        case CodeType::write:
            execute_write(line);
            break;
        case CodeType::pointer_write:
            execute_pointer_write(line);
            break;
        case CodeType::compression:
            execute_compression(line);
            break;
        case CodeType::mov:
            execute_mov(line);
            break;
        case CodeType::pointer_compression:
            execute_pointer_compression(line);
            break;
        case CodeType::pointer_mov:
            execute_pointer_mov(line);
            break;
        case CodeType::arm_write:
            execute_arm_write(line);
            break;
        case CodeType::relative_base:
            execute_relative_base(line);
            break;
        case CodeType::button_pad:
            execute_button_pad(line);
            break;
        default:
            execute_condition(line);
            break;
        }
    }

    // `$0X00 <address> <value>`
    void execute_write(const CodeLine &line) {
        ++m_index;
        write_value(m_mem, resolve(line.first), static_cast<CodeWidth>(line.op()), line.second);
    }

    // `$5X00 <destination> <source>`
    void execute_mov(const CodeLine &line) {
        ++m_index;

        const auto width = static_cast<CodeWidth>(line.op());
        uint32_t value = 0;
        if (read_value(m_mem, resolve(line.second), width, value))
            write_value(m_mem, resolve(line.first), width, value);
    }

    // `$4X01 <address> <value>` followed by `$<count> <address gap> <value gap>`
    void execute_compression(const CodeLine &line) {
        const CodeLine gaps = m_cheat.lines[m_index + 1];
        m_index += 2;

        const auto width = static_cast<CodeWidth>(line.op());
        // The whole control word of the second line is the iteration count.
        const uint32_t count = gaps.control;
        for (uint32_t i = 0; i < count; ++i)
            write_value(m_mem, resolve(line.first) + (i * gaps.first), width, line.second + (i * gaps.second));
    }

    // `$3X<level> <address> <offset>`, one line per further offset, then the value.
    void execute_pointer_write(const CodeLine &line) {
        const PointerBlock block = parse_pointer_block(line);

        uint32_t address = 0;
        if (follow_pointers(block, address))
            write_value(m_mem, address, static_cast<CodeWidth>(line.op()), block.closing.second);
    }

    // `$8X<level> ...` writes to `$8<4|5|6><level> ...`, both blocks closed by a `$88` / `$89` line.
    void execute_pointer_mov(const CodeLine &line) {
        const PointerBlock destination = parse_pointer_block(line);
        const PointerBlock source = parse_pointer_block(m_cheat.lines[m_index]);

        const auto width = static_cast<CodeWidth>(line.op());
        uint32_t destination_address = 0;
        uint32_t source_address = 0;
        if (!follow_pointers(destination, destination_address) || !follow_pointers(source, source_address))
            return;

        uint32_t value = 0;
        if (read_value(m_mem, source_address, width, value))
            write_value(m_mem, destination_address, width, value);
    }

    // A `$3` block followed by the `$<count> <address gap> <value gap>` line of a `$4` code.
    void execute_pointer_compression(const CodeLine &line) {
        const PointerBlock block = parse_pointer_block(line);
        const CodeLine gaps = m_cheat.lines[m_index];
        ++m_index;

        uint32_t address = 0;
        if (!follow_pointers(block, address))
            return;

        const auto width = static_cast<CodeWidth>(line.op());
        const uint32_t count = gaps.control;
        for (uint32_t i = 0; i < count; ++i)
            write_value(m_mem, address + (i * gaps.first), width, block.closing.second + (i * gaps.second));
    }

    // `$AX00 <address> <instruction>`, X is 1 for a 16-bit and 2 for a 32-bit instruction.
    void execute_arm_write(const CodeLine &line) {
        ++m_index;

        const auto width = (line.op() == 1) ? CodeWidth::bits16 : CodeWidth::bits32;
        const uint32_t size = width_size(width);
        const uint32_t address = resolve(line.first);
        const uint32_t value = line.second & width_mask(width);

        uint32_t current = 0;
        if (!read_value(m_mem, address, width, current))
            return;

        // Patching guest code once is enough, only pay for the recompiler flush when it changed.
        if (current == value)
            return;

        const bool already_saved = std::any_of(m_cheat.saved_memory.begin(), m_cheat.saved_memory.end(),
            [address, size](const SavedMemory &saved) { return (saved.address == address) && (saved.bytes.size() == size); });
        if (!already_saved) {
            // Saving the real original makes the order the cheats are turned off in irrelevant.
            SavedMemory saved;
            saved.address = address;
            for (uint32_t i = 0; i < size; ++i)
                saved.bytes.push_back(original_byte(address + i, static_cast<uint8_t>(current >> (i * 8))));
            m_cheat.saved_memory.push_back(std::move(saved));
        }

        if (write_value(m_mem, address, width, value) && m_invalidate_jit)
            m_invalidate_jit(address, size);
    }

    void execute_relative_base(const CodeLine &line) {
        ++m_index;
        m_relative_base = m_state.modules[line.param()].segments[line.first & 0xF].address;
    }

    // `$C2<lines> <pad type> <button mask>`
    void execute_button_pad(const CodeLine &line) {
        ++m_index;

        // The pad type selects between the Vita pad and a DualShock, Vita3K only exposes one pad.
        const uint32_t mask = line.second;
        if ((m_buttons & mask) != mask)
            skip_related_codes(line.param());
    }

    // `$DX<lines> <address> <value>`
    void execute_condition(const CodeLine &line) {
        ++m_index;

        const uint8_t op = line.op();
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
    }

    // Databases word the control field of the follow-up lines freely, so only `second` is read.
    PointerBlock parse_pointer_block(const CodeLine &head) {
        const uint8_t level = head.param();

        PointerBlock block;
        block.address = head.first;
        block.offsets.reserve(level);
        block.offsets.push_back(head.second);
        for (uint8_t i = 1; i < level; i++)
            block.offsets.push_back(m_cheat.lines[m_index + i].second);

        block.closing = m_cheat.lines[m_index + level];
        m_index += level + 1;

        return block;
    }

    bool follow_pointers(const PointerBlock &block, uint32_t &address) {
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

bool overlaps(const SavedMemory &a, const SavedMemory &b) {
    return (a.address < b.address + b.bytes.size()) && (b.address < a.address + a.bytes.size());
}

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

    state.title_id.clear();
    state.file = {};
    state.modules.clear();
    state.frames_presented.store(0, std::memory_order_relaxed);
    state.buttons.store(0, std::memory_order_relaxed);
}

bool load(CheatState &state, const fs::path &cheats_dir, const std::string &title_id) {
    CheatFile file;
    const auto path = find_cheat_file(cheats_dir, title_id);
    if (path.empty()) {
        LOG_DEBUG("No cheat file found for {} in {}", title_id, cheats_dir);
    } else {
        file = parse_cheat_file(path, title_id);
        for (auto &cheat : file.cheats)
            cheat.enabled = cheat.enabled_on_boot;
    }

    const std::lock_guard<std::mutex> lock(state.mutex);

    // Checked here rather than when they run, so that the cheat manager flags a broken cheat before it is turned on.
    for (auto &cheat : file.cheats) {
        if (!cheat.broken)
            cheat.broken = !Validator(cheat, state.modules).validate();
    }

    state.title_id = title_id;
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

    if (state.frames_presented.load(std::memory_order_relaxed) < state.launch_delay_frames)
        return;

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
    if (enabled)
        return;

    const std::vector<SavedMemory> restored = cheat.saved_memory;
    restore_cheat(cheat, mem, invalidate_jit);

    // The game keeps running, so a patch another cheat still wants must not wait for the next vblank.
    const uint32_t buttons = state.buttons.load(std::memory_order_relaxed);
    for (auto &other : state.file.cheats) {
        if (!state.enabled || !other.enabled || other.broken || (&other == &cheat))
            continue;

        const bool shares_memory = std::any_of(other.saved_memory.begin(), other.saved_memory.end(), [&restored](const SavedMemory &saved) {
            return std::any_of(restored.begin(), restored.end(), [&saved](const SavedMemory &old) { return overlaps(saved, old); });
        });
        if (shares_memory)
            Interpreter(state, other, mem, buttons, invalidate_jit).run();
    }
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

std::string loaded_title_id(const CheatState &state) {
    const std::lock_guard<std::mutex> lock(state.mutex);

    return state.title_id;
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
