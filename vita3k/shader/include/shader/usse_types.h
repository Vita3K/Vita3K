#pragma once

#include <shader/types_imm.h>

#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
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

inline ExtPredicate short_predicate_to_ext(ShortPredicate pred) {
    switch (pred) {
    case ShortPredicate::NONE:
        return ExtPredicate::NONE;
    case ShortPredicate::P0:
        return ExtPredicate::P0;
    case ShortPredicate::P1:
        return ExtPredicate::P1;
    case ShortPredicate::NEGP0:
        return ExtPredicate::NEGP0;
    default:
        return ExtPredicate::NONE;
    }
}

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

inline bool is_identical(const Swizzle4 &lhs, const Swizzle4 &rhs, Imm4 dest_mask) {
    for (int i = 0; i < 4; i++) {
        if (dest_mask & (1 << i)) {
            if (lhs[i] != rhs[i]) {
                return false;
            }
        }
    }

    return true;
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

enum class CompareMethod : uint8_t {
    NE_ZERO = 0b00,
    EQ_ZERO = 0b01,
    LT_ZERO = 0b10,
    LTE_ZERO = 0b11,
};

enum class DataType : uint8_t {
    INT8,
    INT16,
    INT32,
    C10,
    F16,
    F32,
    UINT8,
    UINT16,
    UINT32,
    O8,
    TOTAL_TYPE,
    UNK
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
    INDEXED1,
    INDEXED2,
    PREDICATE,

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

inline std::size_t get_data_type_size(const DataType type) {
    switch (type) {
    case DataType::INT8:
    case DataType::UINT8:
    // These two down are not sure
    case DataType::O8:
    case DataType::C10:
        return 1;

    case DataType::INT16:
    case DataType::F16:
    case DataType::UINT16: {
        return 2;
    }

    case DataType::INT32:
    case DataType::F32:
    case DataType::UINT32: {
        return 4;
    }

    default:
        break;
    }

    return 4;
}

inline bool is_unsigned_integer_data_type(const DataType dtype) {
    return (dtype == DataType::UINT16) || (dtype == DataType::UINT32) || (dtype == DataType::UINT8);
}

inline bool is_signed_integer_data_type(const DataType dtype) {
    return (dtype == DataType::INT16) || (dtype == DataType::INT8) || (dtype == DataType::INT32);
}

inline bool is_float_data_type(const DataType dtype) {
    return (dtype == DataType::C10) || (dtype == DataType::F16) || (dtype == DataType::F32);
}

// TODO: Make this a std::set?
enum InstructionFlags {
};

struct Operand {
    Imm6 num = 0b111111;
    RegisterBank bank = RegisterBank::INVALID;
    RegisterFlags flags{};
    Swizzle4 swizzle = SWIZZLE_CHANNEL_4_UNDEFINED;
    DataType type = DataType::F32;

    int index{ 0 };

    bool is_same(const Operand &op, const Imm4 mask) {
        return (op.bank == bank) && (is_identical(swizzle, op.swizzle, mask)) && (num == op.num);
    }
};

struct InstructionOperands {
    Operand dest;
    Operand src0;
    Operand src1;
    Operand src2;

    InstructionOperands() {
        src0.index = 0;
        src1.index = 1;
        src2.index = 2;
        dest.index = 3;
    }
};

struct Instruction {
    Opcode opcode = Opcode::INVALID;
    InstructionOperands opr;
    uint32_t flags;
    Imm4 dest_mask;
    RepeatCount repeat_count{ RepeatCount::REPEAT_0 };

    Instruction()
        : opr()
        , flags(0)
        , dest_mask(0) {
    }
};

/**
 * Container type for gxp parameters
 */
enum class GenericType {
    SCALER,
    VECTOR,
    ARRAY,
    INVALID
};

/**
 * Samplers that are attached to gxp shader program
 */
struct Sampler {
    std::string name;
    uint32_t index; // resource index
};

/**
 * Uniform buffers that are attached to gxp shader program
 */
struct UniformBuffer {
    uint32_t index;
    uint32_t size;
    uint32_t reg_start_offset;
    uint32_t reg_block_size;
    bool rw; // TODO confirm this
};

struct UniformInputSource {
    std::string name;
    // resource index
    uint32_t index;
    bool in_mem;
};

struct AttributeInputSoucre {
    std::string name;
    // resource index
    uint32_t index;
};

struct LiteralInputSource {
    // The constant data
    float data;
};

struct UniformBufferInputSource {
    uint32_t base;
    // resource index
    uint32_t index;
};

struct DependentSamplerInputSource {
    std::string name;
    uint32_t index; // resource index
};

// Read source field in Input struct
using InputSource = std::variant<UniformInputSource, UniformBufferInputSource, LiteralInputSource, AttributeInputSoucre, DependentSamplerInputSource>;

/**
 * Input parameters that are usually copied into PA or SA
 * registers before shader program starts.
 */
struct Input {
    // Destination reigster bank
    RegisterBank bank;
    // The type of parameter
    DataType type;
    // The container type of parameter
    GenericType generic_type;
    // Offset of destination register
    uint32_t offset;
    uint32_t component_count;
    uint32_t array_size;
    // The source where shader fetches the data
    // e.g. vertex attributes
    InputSource source;
};

/**
 * Contains the metadata of various input data of gxp shader program
 */
struct ProgramInput {
    std::vector<Input> inputs;
    std::vector<Sampler> samplers;
    std::vector<UniformBuffer> uniform_buffers;
};

enum class ShaderPhase {
    SampleRate, // Secondary phase
    Pixel, // Primary phase

    Max,
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
