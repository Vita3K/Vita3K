#pragma once

#include <shader/types_imm.h>

#include <array>
#include <functional>
#include <vector>

namespace shader {
namespace usse {

enum class Opcode {
#define OPCODE(n) n,
#include "usse_opcodes.inc"
#undef OPCODE
};

enum class ExtPredicate : uint8_t {
    NONE,
    P0,
    P1,
    P2,
    P3,
    NEGP0,
    NEGP1,
    PN
};

enum class ShortPredicate : uint8_t {
    NONE,
    P0,
    P1,
    NEGP0,
};

// NOTE: prepended with "_" to allow for '1' and '2' (so that everything is 1 character long)
enum class SwizzleChannel {
    _X,
    _Y,
    _Z,
    _W,
    _0, // zero
    _1, // one
    _2, // two
    _H, // half

    _UNDEFINED,
};

template <unsigned N>
using Swizzle = std::array<SwizzleChannel, N>;

using Swizzle3 = Swizzle<3>;
using Swizzle4 = Swizzle<4>;

#define SWIZZLE_CHANNEL_3(ch1, ch2, ch3) \
    { SwizzleChannel::_##ch1, SwizzleChannel::_##ch2, SwizzleChannel::_##ch3 }
#define SWIZZLE_CHANNEL_4(ch1, ch2, ch3, ch4) \
    { SwizzleChannel::_##ch1, SwizzleChannel::_##ch2, SwizzleChannel::_##ch3, SwizzleChannel::_##ch4 }
#define SWIZZLE_CHANNEL_4_CAST(ch1, ch2, ch3, ch4) \
    { static_cast<SwizzleChannel>(ch1), static_cast<SwizzleChannel>(ch2), static_cast<SwizzleChannel>(ch3), static_cast<SwizzleChannel>(ch4) }

#define SWIZZLE_CHANNEL_4_UNDEFINED SWIZZLE_CHANNEL_4(UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED)
#define SWIZZLE_CHANNEL_3_UNDEFINED SWIZZLE_CHANNEL_3(UNDEFINED, UNDEFINED, UNDEFINED)

#define SWIZZLE_CHANNEL_4_DEFAULT SWIZZLE_CHANNEL_4(X, Y, Z, W)
#define SWIZZLE_CHANNEL_3_DEFAULT SWIZZLE_CHANNEL_3(X, Y, Z)

inline Swizzle4 to_swizzle4(Swizzle3 sw) {
    return Swizzle4{ sw[0], sw[1], sw[2], SwizzleChannel::_X };
}

inline bool is_default(Swizzle4 sw, Imm4 sw_len = 4) {
    bool res = true;

    // clang-format off
    switch (sw_len) {
    case 4: if (sw[3] != SwizzleChannel::_W) res = false; // fallthrough
    case 3: if (sw[2] != SwizzleChannel::_Z) res = false; // fallthrough
    case 2: if (sw[1] != SwizzleChannel::_Y) res = false; // fallthrough
    case 1: if (sw[0] != SwizzleChannel::_X) res = false; // fallthrough
    }
    // clang-format on

    return res;
}

enum class RepeatCount : uint8_t {
    REPEAT_0,
    REPEAT_1,
    REPEAT_2,
    REPEAT_3,
};

enum class MoveType : uint8_t {
    UNCONDITIONAL,
    CONDITIONAL,
    CONDITIONALU8,
};

enum class MoveDataType : uint8_t {
    INT8,
    INT16,
    INT32,
    C10,
    F16,
    F32,
};

enum class SpecialCategory : uint8_t {
};

enum class RegisterBank {
    TEMP,
    PRIMATTR,
    OUTPUT,
    SECATTR,
    FPINTERNAL,
    SPECIAL,
    GLOBAL,
    FPCONSTANT,
    IMMEDIATE,
    INDEX,
    INDEXED,

    MAXIMUM,
    INVALID
};

// TODO: Make this a std::set?
enum RegisterFlags : uint32_t {
    Absolute = 0x1000, ///< Absolute the value
    Negative = 0x2000 ///< Negate the value
};

inline RegisterFlags operator|(RegisterFlags a, RegisterFlags b) {
    return (RegisterFlags)((uint32_t)a | (uint32_t)b);
}

inline RegisterFlags operator&(RegisterFlags a, RegisterFlags b) {
    return (RegisterFlags)((uint32_t)a & (uint32_t)b);
}

inline RegisterFlags &operator|=(RegisterFlags &a, RegisterFlags b) {
    return (RegisterFlags &)((uint32_t &)a |= (uint32_t)b);
}

inline RegisterFlags &operator&=(RegisterFlags &a, RegisterFlags b) {
    return (RegisterFlags &)((uint32_t &)a &= (uint32_t)b);
}

// TODO: Make this a std::set?
enum InstructionFlags {
};

struct Operand {
    Imm6 num = 0b111111;
    RegisterBank bank = RegisterBank::INVALID;
    RegisterFlags flags{};
    Swizzle4 swizzle = SWIZZLE_CHANNEL_4_UNDEFINED;
};

struct InstructionOperands {
    Operand dest;
    Operand src0;
    Operand src1;
    Operand src2;
};

struct Instruction {
    Opcode opcode = Opcode::INVALID;
    InstructionOperands opr;
    uint32_t flags;
    Imm4 dest_mask;
    RepeatCount repeat_count{ RepeatCount::REPEAT_0 };
};

} // namespace usse
} // namespace shader

namespace std {

// Needed for translating opcode enums to strings
template <>
struct hash<shader::usse::Opcode> {
    std::size_t operator()(const shader::usse::Opcode &e) const {
        return static_cast<std::size_t>(e);
    }
};

} // namespace std
