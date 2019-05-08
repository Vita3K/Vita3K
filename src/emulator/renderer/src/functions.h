#pragma once

#include <crypto/hash.h>
#include <glutil/object.h>
#include <gxm/types.h>
#include <renderer/types.h>

#include <psp2/gxm.h>

#include <map>
#include <string>
#include <tuple>

struct GxmContextState;
struct MemState;

namespace renderer {
struct TextureCacheState;

typedef std::tuple<std::string, std::string> ProgramGLSLs;
typedef std::map<ProgramGLSLs, SharedGLObject> ProgramCache;

// Attribute formats.
size_t attribute_format_size(SceGxmAttributeFormat format);
GLenum attribute_format_to_gl_type(SceGxmAttributeFormat format);
GLboolean attribute_format_normalised(SceGxmAttributeFormat format);

// Compile program.
SharedGLObject compile_program(ProgramCache &cache, const GxmContextState &state, const MemState &mem);

// Shaders.
GLSLCacheEntry load_shader(GLSLCache &cache, const SceGxmProgram &program, const char *base_path, const char *title_id);

namespace texture {

// Textures.
void bind_texture(TextureCacheState &cache, const SceGxmTexture &gxm_texture, const MemState &mem);
void configure_bound_texture(const SceGxmTexture &gxm_texture);
void upload_bound_texture(const SceGxmTexture &gxm_texture, const MemState &mem);

// Texture formats.
const GLenum *translate_swizzle(SceGxmTextureFormat fmt);
GLenum translate_internal_format(SceGxmTextureFormat src);
GLenum translate_format(SceGxmTextureFormat src);
GLenum translate_type(SceGxmTextureFormat format);
GLenum translate_wrap_mode(SceGxmTextureAddrMode src);
GLenum translate_minmag_filter(SceGxmTextureFilter src);
size_t bits_per_pixel(SceGxmTextureBaseFormat base_format);

// Paletted textures.
void palette_texture_to_rgba_4(uint32_t *dst, const uint8_t *src, size_t width, size_t height, const uint32_t *palette);
void palette_texture_to_rgba_8(uint32_t *dst, const uint8_t *src, size_t width, size_t height, const uint32_t *palette);
const uint32_t *get_texture_palette(const SceGxmTexture &texture, const MemState &mem);

// Texture cache.
bool init(TextureCacheState &cache);
void cache_and_bind_texture(TextureCacheState &cache, const SceGxmTexture &gxm_texture, const MemState &mem);

} // namespace texture

// Uniforms.
void set_uniforms(GLuint program, const GxmContextState &state, const MemState &mem, bool log_uniforms);
} // namespace renderer
