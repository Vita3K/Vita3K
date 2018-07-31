#pragma once

#include <psp2/gxm.h>

struct GxmContextState;
struct MemState;
struct SDL_Window;

namespace emu {
struct SceGxmBlendInfo;
}

namespace renderer {
struct Context;
struct FragmentProgram;
struct RenderTarget;
struct State;
struct VertexProgram;

bool create(Context &context, SDL_Window *window);
bool create(RenderTarget &rt, const SceGxmRenderTargetParams &params);
bool create(FragmentProgram &fp, State &state, const SceGxmProgram &program, const emu::SceGxmBlendInfo *blend, const char *base_path);
bool create(VertexProgram &vp, State &state, const SceGxmProgram &program, const char *base_path);
void begin_scene(const RenderTarget &rt);
void end_scene(Context &context, size_t width, size_t height, size_t stride_in_pixels, uint32_t *pixels);
bool sync_state(Context &context, const GxmContextState &state, const MemState &mem, bool enable_texture_cache);
void draw(Context &context, const GxmContextState &state, SceGxmPrimitiveType type, SceGxmIndexFormat format, const void *indices, size_t count, const MemState &mem);
void finish(Context &context);
} // namespace renderer
