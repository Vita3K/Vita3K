#pragma once

#include <psp2/gxm.h>

struct GxmContextState;
struct MemState;

namespace renderer {
    struct Context;
    struct RenderTarget;
    struct State;
    
    bool create(Context &context);
    bool create(RenderTarget &rt, const SceGxmRenderTargetParams &params);
    void begin_scene(const RenderTarget &rt);
    void end_scene(Context &context, size_t width, size_t height, size_t stride_in_pixels, uint32_t *pixels);
    bool sync_state(Context &context, const GxmContextState &state, const MemState &mem, bool enable_texture_cache);
    void draw(Context &context, const GxmContextState &state, SceGxmPrimitiveType type, SceGxmIndexFormat format, const void *indices, size_t count, const MemState &mem);
    void finish(Context &context);
}
