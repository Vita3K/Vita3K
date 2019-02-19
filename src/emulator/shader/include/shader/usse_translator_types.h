#pragma once

#include <spirv_glsl.hpp>

namespace shader::usse {

using SpirvCode = std::vector<uint32_t>;

struct SpirvVar {
    spv::Id type_id{};
    spv::Id var_id{};
};

struct SpirvReg {
    spv::Id type_id{};
    spv::Id var_id{};

    uint32_t offset{};
    uint32_t size{};
};

// Helper for managing USSE registers and register banks and their associated SPIR-V variables
struct SpirvVarRegBank {
    SpirvVarRegBank() = default;

    bool find_reg_at(uint32_t index, SpirvReg &out_reg, uint32_t &out_comp_offset) const {
        for (auto var : vars) {
            if (index >= var.offset && index < var.offset + var.size) {
                out_reg = var;
                out_comp_offset = index - var.offset;
                return true;
            }
        }
        return false;
    }

    std::vector<SpirvReg> &get_vars() {
        return vars;
    }
    const std::vector<SpirvReg> &get_vars() const {
        return vars;
    }

    void push(const SpirvVar &var, uint32_t size) {
        const auto offset = next_offset;

        vars.push_back({ var.type_id, var.var_id, offset, size });
        next_offset += size;
    }

    size_t size() const {
        size_t size = 0;
        for (auto var : vars) {
            size += var.size;
        }
        return size;
    }

private:
    std::vector<SpirvReg> vars{};
    uint32_t next_offset{};
};

struct SpirvShaderParameters {
    // Mapped to 'pa' (primary attribute) USSE registers
    // for vertex: vertex inputs (vertex attributes)
    // for fragment: fragment inputs (linkage from vertex stage)
    SpirvVarRegBank ins;

    // Mapped to 'sa' (secondary attribute) USSE registers
    SpirvVarRegBank uniforms;

    // Mapped to 'r' (temporary) USSE registers
    SpirvVarRegBank temps;

    // Mapped to 'i' (internal) usse registers
    SpirvVarRegBank internals;

    // Mapped to 'o' (output) USSE registers
    // for vertex: vertex outputs (linkage into fragment stage)
    // for fragment: fragment outputs (color outputs)
    SpirvVarRegBank outs;

    // Struct metadata, unused atm
    SpirvVarRegBank structs;
};

} // namespace shader::usse
