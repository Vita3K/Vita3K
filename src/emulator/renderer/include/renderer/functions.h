#pragma once

#include <renderer/types.h>

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
bool create(FragmentProgram &fp, State &state, const SceGxmProgram &program, const emu::SceGxmBlendInfo *blend, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id);
bool create(VertexProgram &vp, State &state, const SceGxmProgram &program, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id);
void begin_scene(const RenderTarget &rt);
void end_scene(Context &context, SceGxmSyncObject *sync_object, size_t width, size_t height, size_t stride_in_pixels, uint32_t *pixels);
bool sync_state(Context &context, const GxmContextState &state, const MemState &mem, bool enable_texture_cache, bool log_active_shaders, bool log_uniforms);
void draw(Context &context, const GxmContextState &state, SceGxmPrimitiveType type, SceGxmIndexFormat format, const void *indices, size_t count, const MemState &mem);
void finish(Context &context);
void wait_sync_object(SceGxmSyncObject *sync_object);
} // namespace renderer
