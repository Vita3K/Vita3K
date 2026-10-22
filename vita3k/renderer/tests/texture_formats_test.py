from pathlib import Path
import subprocess
import argparse
import tempfile

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--cxx', default='c++')
parser.add_argument('--dependencies', type=Path, default=ROOT / 'external')
parser.add_argument('--format-source', type=Path)
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
_temporary = tempfile.TemporaryDirectory(prefix='vita3k-texture-formats-')
SANITIZERS = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all'] if args.sanitize else []
OUT = Path(_temporary.name)


def function(text: str, signature: str) -> str:
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    for end in range(opening + 1, len(text)):
        depth += (text[end] == '{') - (text[end] == '}')
        if depth == 0:
            return text[start:end + 1]
    raise ValueError('Unterminated function: ' + signature)


vk_source = (ROOT / 'vita3k/renderer/src/vulkan/texture.cpp').read_text()
start = vk_source.index('\n', vk_source.index('    support_a2rgb10 =')) + 1
end = vk_source.index('    // check for dxt support', start)
capability = vk_source[start:end]
cache_source = (ROOT / 'vita3k/renderer/src/texture/cache.cpp').read_text()
start = cache_source.index('        case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:',
                           cache_source.index('void TextureCache::upload_texture('))
end = cache_source.index('        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:', start)
conversion = cache_source[start:end]
mapper = function((ROOT / 'vita3k/renderer/src/vulkan/gxm_to_vulkan.cpp').read_text(),
                  'vk::Format translate_format(SceGxmTextureBaseFormat base_format)')
mapper = mapper.replace('"../texture/astc_formats.inc"',
                        '"' + str(ROOT / 'vita3k/renderer/src/texture/astc_formats.inc') + '"')
decoder = function((ROOT / 'vita3k/renderer/src/texture/format.cpp').read_text(),
                   'uint32_t decompress_compressed_texture(')
bpp = function((ROOT / 'vita3k/gxm/src/textures.cpp').read_text(),
               'uint32_t bits_per_pixel(SceGxmTextureBaseFormat base_format)')
mapping_source = (args.format_source or ROOT / 'vita3k/renderer/src/vulkan/gxm_to_vulkan.cpp').read_text()
constants = mapping_source[mapping_source.index('using Swizzle ='):mapping_source.index('namespace color {')]
helpers = mapping_source[mapping_source.index('namespace texture {'):mapping_source.index('vk::Format translate_format(SceGxmTextureBaseFormat base_format)')]
base_format = function((ROOT / 'vita3k/gxm/src/textures.cpp').read_text(),
                       'SceGxmTextureBaseFormat get_base_format(SceGxmTextureFormat src)')
program = r'''
#define VK_NO_PROTOTYPES
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <gxm/types.h>
#include <renderer/pvrt-dec.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#define LOG_ERROR(...) do {} while (0)
#define LOG_ERROR_ONCE(...) do {} while (0)
#define LOG_INFO_ONCE(...) do {} while (0)
namespace gxm {
''' + bpp + r'''
}
struct Device {
    bool advertised;
    vk::FormatProperties getFormatProperties(vk::Format) const {
        vk::FormatProperties properties{};
        if (advertised)
            properties.optimalTilingFeatures = vk::FormatFeatureFlagBits::eSampledImage;
        return properties;
    }
};
static bool native_upload(bool advertised) {
    struct { Device physical_device; } state{{advertised}};
    bool support_pvrt = true;
''' + capability + r'''
    return support_pvrt;
}
static void decompress_bc_image(uint32_t, uint32_t, const uint8_t *, uint32_t *, uint8_t) {
    assert(false && "The PVRTC fixture must not use a BC decoder");
}
''' + decoder + '\n' + mapper + r'''
static unsigned checks;
static void verify(bool advertised, SceGxmTextureBaseFormat base_format,
                   uint32_t width, uint32_t height, uint32_t color,
                   std::array<uint8_t, 4> expected) {
    const bool two_bit = gxm::bits_per_pixel(base_format) == 2;
    const uint32_t padded_width = std::max(width, two_bit ? 16u : 8u);
    const uint32_t padded_height = std::max(height, 8u);
    std::vector<uint32_t> compressed(padded_width * padded_height * (two_bit ? 2 : 4) / 32);
    for (size_t i = 0; i < compressed.size(); i += 2) {
        compressed[i] = 0;
        compressed[i + 1] = color;
    }
    const auto original = compressed;
    const bool support_pvrt = native_upload(advertised);
    const bool is_swizzled = true;
    const uint32_t pixels_per_stride = width;
    const uint32_t memory_height = height;
    const void *pixels = compressed.data();
    uint32_t bytes_per_pixel = 1;
    uint32_t bpp = gxm::bits_per_pixel(base_format);
    auto upload_format = base_format;
    std::vector<uint8_t> texture_data_decompressed;
    switch (base_format) {
''' + conversion + r'''
    default:
        assert(false);
    }
    if (upload_format != SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8) {
        std::fprintf(stderr, "PVRTC FORMAT MISMATCH: advertised=%u base=%08x output_bpp=%u, Vulkan image is RGBA8\n",
                     advertised, uint32_t(base_format), bpp);
        std::abort();
    }
    assert(translate_format(base_format) == vk::Format::eR8G8B8A8Unorm);
    assert(translate_format(upload_format) == translate_format(base_format));
    assert(gxm::bits_per_pixel(upload_format) == 32 && bpp == 32 && bytes_per_pixel == 4);
    assert(texture_data_decompressed.size() == size_t(width) * height * vk::blockSize(translate_format(base_format)));
    assert(pixels == texture_data_decompressed.data());
    for (size_t i = 0; i < size_t(width) * height; ++i) {
        const auto *actual = static_cast<const uint8_t *>(pixels) + 4 * i;
        if (!std::equal(expected.begin(), expected.end(), actual)) {
            std::fprintf(stderr, "COLOR MISMATCH: base=%08x %ux%u block=%08x got=%u,%u,%u,%u expected=%u,%u,%u,%u\n",
                         uint32_t(base_format), width, height, color, actual[0], actual[1], actual[2], actual[3],
                         expected[0], expected[1], expected[2], expected[3]);
            std::abort();
        }
    }
    assert(compressed == original);
    ++checks;
}
int main() {
    const std::array formats{SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP, SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP,
                            SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP, SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP};
    const std::array<std::pair<uint32_t, std::array<uint8_t, 4>>, 5> colors{{
        {0xffe0ffe0, {255, 255, 0, 255}},
        {0xfc00fc00, {255, 0, 0, 255}},
        {0x83e083e0, {0, 255, 0, 255}},
        {0x801f801e, {0, 0, 255, 255}},
        {0, {0, 0, 0, 0}}
    }};
    const std::array<std::pair<uint32_t, uint32_t>, 7> sizes{{{1,1}, {2,2}, {4,4}, {8,8}, {16,8}, {16,16}, {32,16}}};
    for (bool advertised : {false, true})
        for (auto format : formats)
            for (const auto &[color, expected] : colors)
                for (const auto &[width, height] : sizes)
                    verify(advertised, format, width, height, color, expected);
    std::printf("%u PVRTC capability/conversion cases passed: four variants, GPU support on/off, RGB/alpha, dimensions and source preservation\n", checks);
}
'''

swizzle_test = r"""
static void verify_rgba4444() {
    // eR4G4B4A4UnormPack16 decodes 0x1234 into native components {1,2,3,4}.
    // GXM format names describe the packed word from most to least significant field.
    const std::array<std::pair<SceGxmTextureFormat, std::array<int, 4>>, 4> cases{{
        { SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_RGBA, {1, 2, 3, 4} },
        { SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_ABGR, {4, 3, 2, 1} },
        { SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_ARGB, {2, 3, 4, 1} },
        { SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_BGRA, {3, 2, 1, 4} }
    }};
    for (const auto &[format, expected] : cases) {
        const auto mapping = texture::translate_swizzle(format);
        const auto value = [](vk::ComponentSwizzle component) {
            switch (component) {
            case vk::ComponentSwizzle::eR: return 1;
            case vk::ComponentSwizzle::eG: return 2;
            case vk::ComponentSwizzle::eB: return 3;
            case vk::ComponentSwizzle::eA: return 4;
            case vk::ComponentSwizzle::eZero: return 0;
            case vk::ComponentSwizzle::eOne: return 15;
            default: std::abort();
            }
        };
        const std::array<int, 4> actual{value(mapping.r), value(mapping.g), value(mapping.b), value(mapping.a)};
        assert(actual == expected);
    }
    std::puts("Four packed RGBA4444 swizzle cases passed");
}
"""
extra = 'namespace gxm {\n' + base_format + '\n}\n' + constants + helpers + '\n}\n' + swizzle_test
program = program.replace('int main() {', extra + '\nint main() {\n    verify_rgba4444();', 1)
OUT.mkdir(parents=True, exist_ok=True)
source = OUT / 'pvrtc-upload-test.cpp'
source.write_text(program)
command = [args.cxx, '-std=c++23', '-O1', '-g', *SANITIZERS]
for directory in ('external/VulkanMemoryAllocator-Hpp/Vulkan-Headers/include', 'vita3k/gxm/include',
                  'vita3k/util/include', 'vita3k/renderer/include'):
    command += ['-I', str(args.dependencies / directory[len('external/'):]) if directory.startswith('external/') else str(ROOT / directory)]
command += [str(source), str(ROOT / 'vita3k/renderer/src/texture/pvrt-dec.cpp'), '-o', str(OUT / 'pvrtc-upload-test')]
subprocess.run(command, check=True)
subprocess.run([str(OUT / 'pvrtc-upload-test')], check=True)
