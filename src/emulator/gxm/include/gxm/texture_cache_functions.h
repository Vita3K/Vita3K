#pragma once

struct MemState;
struct SceGxmTexture;
struct TextureCacheState;

bool init(TextureCacheState &cache);
void cache_and_bind_texture(TextureCacheState &cache, const SceGxmTexture &gxm_texture, const MemState &mem, bool enabled);
