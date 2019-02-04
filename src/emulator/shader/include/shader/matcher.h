/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * This software may be used and distributed according to the terms of the GNU
 * General Public License version 2 or any later version.
 */

#pragma once

#include <cassert>
#include <functional>

namespace shader {
namespace decoder {

/**
 * Generic instruction handling construct.
 *
 * @tparam Visitor An arbitrary visitor type that will be passed through
 *                 to the function being handled. This type must be the
 *                 type of the first parameter in a handler function.
 *
 * @tparam OpcodeType Type representing an opcode. This must be the
 *                    type of the second parameter in a handler function.
 */
template <typename Visitor, typename OpcodeType>
class Matcher {
public:
    using opcode_type = OpcodeType;
    using visitor_type = Visitor;
    using handler_return_type = typename Visitor::instruction_return_type;
    using handler_function = std::function<handler_return_type(Visitor &, opcode_type)>;

    Matcher(const char *const name, opcode_type mask, opcode_type expected, handler_function func)
        : name{ name }
        , mask{ mask }
        , expected{ expected }
        , fn{ std::move(func) } {}

    /// Gets the name of this type of instruction.
    const char *GetName() const {
        return name;
    }

    /// Gets the mask for this instruction.
    opcode_type GetMask() const {
        return mask;
    }

    /// Gets the expected value after masking for this instruction.
    opcode_type GetExpected() const {
        return expected;
    }

    /**
     * Tests to see if the given instruction is the instruction this matcher represents.
     * @param instruction The instruction to test
     * @returns true if the given instruction matches.
     */
    bool Matches(opcode_type instruction) const {
        return (instruction & mask) == expected;
    }

    /**
     * Calls the corresponding instruction handler on visitor for this type of instruction.
     * @param v The visitor to use
     * @param instruction The instruction to decode.
     */
    handler_return_type call(Visitor &v, opcode_type instruction) const {
        assert(Matches(instruction));
        return fn(v, instruction);
    }

private:
    const char *name;
    opcode_type mask;
    opcode_type expected;
    handler_function fn;
};

} // namespace decoder
} // namespace shader
