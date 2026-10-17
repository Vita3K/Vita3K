// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector> // Dynarmic's ir_emitter.h uses std::vector without including it.

#include <dynarmic/frontend/A32/a32_ir_emitter.h>
#include <dynarmic/frontend/A32/a32_types.h>
#include <dynarmic/ir/acc_type.h>
#include <dynarmic/ir/basic_block.h>
#include <dynarmic/ir/terminal.h>

#include <cstdint>
#include <optional>

struct AliasedThumbInstruction {
    enum class Kind { Store,
        ExclusiveStore } kind;
    unsigned base;
    unsigned source;
    unsigned status;
    unsigned offset;
    bool add;
};

inline std::optional<AliasedThumbInstruction> decode_aliased_thumb(uint16_t first, uint16_t second) {
    const unsigned base = first & 0xF;
    const unsigned source = second >> 12;
    if (base == 15 || source == 15)
        return std::nullopt;

    if ((first & 0xFFF0) == 0xF840 && (second & 0x0D00) == 0x0900 && base == source) {
        return AliasedThumbInstruction{ AliasedThumbInstruction::Kind::Store, base, source, 0,
            unsigned(second & 0xFF), (second & 0x0200) != 0 };
    }

    const unsigned status = (second >> 8) & 0xF;
    if ((first & 0xFFF0) == 0xE840 && status != 15 && (status == source || status == base)
        && !(status == source && status == base)) {
        return AliasedThumbInstruction{ AliasedThumbInstruction::Kind::ExclusiveStore, base, source, status,
            unsigned(second & 0xFF) << 2, true };
    }
    return std::nullopt;
}

inline void translate_aliased_thumb(Dynarmic::A32::IREmitter &ir, const AliasedThumbInstruction &instruction) {
    using Dynarmic::A32::Reg;
    namespace IR = Dynarmic::IR;

    const auto next = ir.current_location.AdvancePC(4).AdvanceIT();
    const auto condition = ir.current_location.IT().Cond();
    if (condition != IR::Cond::AL) {
        ir.block.SetCondition(condition);
        ir.block.SetConditionFailedLocation(next);
        ir.block.ConditionFailedCycleCount() = 1;
    }

    const auto base = ir.GetRegister(static_cast<Reg>(instruction.base));
    const auto value = ir.GetRegister(static_cast<Reg>(instruction.source));
    if (instruction.kind == AliasedThumbInstruction::Kind::Store) {
        ir.WriteMemory32(base, value, IR::AccType::NORMAL);
        const auto offset = ir.Imm32(instruction.offset);
        ir.SetRegister(static_cast<Reg>(instruction.base), instruction.add ? ir.Add(base, offset) : ir.Sub(base, offset));
    } else {
        const auto address = ir.Add(base, ir.Imm32(instruction.offset));
        const auto result = ir.ExclusiveWriteMemory32(address, value, IR::AccType::ATOMIC);
        ir.SetRegister(static_cast<Reg>(instruction.status), result);
    }

    ir.current_location = next;
    ++ir.block.CycleCount();
    ir.SetTerm(IR::Term::LinkBlock{ next });
}

template <typename ReadCode, typename Trace>
bool intercept_aliased_thumb(bool is_thumb, uint32_t pc, Dynarmic::A32::IREmitter &ir, ReadCode read_code, Trace trace) {
    if (!is_thumb || ir.current_location.IT().Cond() == Dynarmic::IR::Cond::NV)
        return true;

    const auto first_word = read_code(pc & ~3U);
    if (!first_word)
        return true;
    const uint16_t first = static_cast<uint16_t>(*first_word >> ((pc & 2U) * 8));
    if ((first & 0xFFF0) != 0xF840 && (first & 0xFFF0) != 0xE840)
        return true;
    const auto second_word = (pc & 2U) ? read_code((pc & ~3U) + 4) : first_word;
    if (!second_word)
        return true;
    const uint16_t second = static_cast<uint16_t>(*second_word >> ((pc & 2U) ? 0 : 16));
    const auto instruction = decode_aliased_thumb(first, second);
    if (!instruction)
        return true;

    // Start a separate block so an IT condition cannot affect preceding instructions.
    if (ir.block.Location() != static_cast<Dynarmic::IR::LocationDescriptor>(ir.current_location)) {
        ir.SetTerm(Dynarmic::IR::Term::LinkBlockFast{ ir.current_location });
        return false;
    }
    trace();
    translate_aliased_thumb(ir, *instruction);
    return false;
}
