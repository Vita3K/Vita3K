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

#pragma once

#include <renderer/types.h>

#include <cstdint>
#include <memory>

struct MemState;

namespace renderer::metal {

struct MetalState;
struct MetalContextImpl;
struct MetalRenderTargetImpl;

// Blend state stored as plain integers holding MTLBlendFactor / MTLBlendOperation / MTLColorWriteMask values
struct BlendState {
    uint32_t enable = 0;
    uint32_t src_color = 0;
    uint32_t dst_color = 0;
    uint32_t color_op = 0;
    uint32_t src_alpha = 0;
    uint32_t dst_alpha = 0;
    uint32_t alpha_op = 0;
    uint32_t write_mask = 0;
};

struct MetalContext : public renderer::Context {
    MetalState &state;
    MemState &mem;
    std::unique_ptr<MetalContextImpl> impl;

    MetalContext(MetalState &state, MemState &mem);
    ~MetalContext() override;
};

struct MetalRenderTarget : public renderer::RenderTarget {
    uint16_t width = 0;
    uint16_t height = 0;
    std::unique_ptr<MetalRenderTargetImpl> impl;

    MetalRenderTarget();
    ~MetalRenderTarget() override;
};

struct MetalFragmentProgram : public renderer::FragmentProgram {
    BlendState blending{};
    uint64_t blending_hash = 0;
};

} // namespace renderer::metal
