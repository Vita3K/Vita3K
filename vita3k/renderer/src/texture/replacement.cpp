// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include "renderer/functions.h"
#include "renderer/texture_cache.h"

#include "gxm/functions.h"
#include "util/align.h"
#include "util/float_to_half.h"
#include "util/log.h"

#include <ddspp.h>
#include <fmt/format.h>
#include <stb_image.h>
#include <stb_image_write.h>

#ifdef __ANDROID__
// for message popup
#include <SDL3/SDL_system.h>
#endif

static constexpr bool log_texture_import = false;
static constexpr bool log_texture_export = true;

namespace renderer {

static SceGxmTextureBaseFormat dxgi_to_gxm(const ddspp::DXGIFormat format);
static ddspp::DXGIFormat gxm_to_dxgi(const SceGxmTextureBaseFormat format);
static ddspp::DXGIFormat dxgi_apply_srgb(const ddspp::DXGIFormat format);

static bool allowed_png_textures(const SceGxmTextureBaseFormat format);
static bool allowed_dds_textures(const SceGxmTextureBaseFormat format);

// copy while applying swizzle to a texture
// sizek is the size of the kth-component in the type
// T is a type that contains exactly one pixel (so uint16_t for 16-bit pixels)
// if reverse is set to true, rgba will be turned into abgr
// if swap_rb is set to true, rgba will be turned into bgra
template <typename T, size_t size1, size_t size2, size_t size3, size_t size4>
static void apply_swizzle_4(const void *src, void *dst, uint32_t nb_pixels, bool reverse, bool swap_rb);

// reverse the components of a 3-component texture (rgb -> bgr)
template <typename T, size_t size1, size_t size2, size_t size3>
static void reverse_comp3_order(const void *src, void *dst, uint32_t nb_pixels);

// some dds format are encoded in a bgra way, in this case the swizzle must be changed
static bool dds_swap_rb(const ddspp::DXGIFormat format);

void TextureCache::set_replacement_state(bool import_textures, bool export_textures, bool export_as_png) {
    if (this->import_textures == import_textures
        && this->export_textures == export_textures
        && this->save_as_png == export_as_png)
        return;

    this->import_textures = import_textures;
    this->export_textures = export_textures;
    this->save_as_png = export_as_png;

    // invalidate all the current textures, will force all of them to be re-uploaded next frame
    for (auto &queue_item : texture_queue.items) {
        queue_item.content.hash = 0;
        queue_item.content.dirty = true;
    }

    refresh_available_textures();
}

void TextureCache::export_select(const SceGxmTexture &texture) {
    const SceGxmTextureBaseFormat format = gxm::get_base_format(gxm::get_format(texture));
    const bool is_cube = texture.texture_type() == SCE_GXM_TEXTURE_CUBE || texture.texture_type() == SCE_GXM_TEXTURE_CUBE_ARBITRARY;

    if (is_cube && save_as_png) {
        LOG_WARN_ONCE("Cubemaps can only be exported as a dds file");
        return;
    }

    if (save_as_png && !allowed_png_textures(format))
        return;
    else if (!save_as_png && !allowed_dds_textures(format))
        return;

    if (exported_textures_hash.find(current_info->hash) != exported_textures_hash.end())
        // texture was already exported
        return;

    if (!save_as_png) {
        if (!dds_descriptor)
            dds_descriptor = new ddspp::Descriptor;

        ddspp::DXGIFormat dxgi_format = gxm_to_dxgi(format);
        if (dxgi_format == ddspp::UNKNOWN)
            return;

        if (texture.gamma_mode != 0)
            dxgi_format = dxgi_apply_srgb(dxgi_format);
        const uint32_t width = gxm::get_width(texture);
        const uint32_t height = gxm::get_height(texture);
        const uint32_t mipcount = texture::get_upload_mip(texture.true_mip_count(), width, height);
        const uint32_t array_size = is_cube ? 6 : 1;
        ddspp::TextureType texture_type = is_cube ? ddspp::Cubemap : ddspp::Texture2D;
        ddspp::Header header = {};
        ddspp::HeaderDXT10 dxt_header = {};
        ddspp::encode_header(dxgi_format, width, height, 1, texture_type, mipcount, array_size, header, dxt_header);

        // we need to do this to get the descriptor anyway
        std::vector<uint8_t> file_header(ddspp::MAX_HEADER_SIZE);
        memcpy(file_header.data(), &ddspp::DDS_MAGIC, sizeof(ddspp::DDS_MAGIC));
        memcpy(file_header.data() + sizeof(ddspp::DDS_MAGIC), &header, sizeof(header));
        memcpy(file_header.data() + sizeof(ddspp::DDS_MAGIC) + sizeof(header), &dxt_header, sizeof(dxt_header));

        ddspp::decode_header(file_header.data(), *dds_descriptor);

        const std::string file_name = fmt::format("{:016X}.dds", current_info->hash);
        fs::path export_name = export_folder / file_name;
        output_file.open(export_name, std::ios_base::binary | std::ios_base::trunc);

        if (!output_file.is_open()) {
            LOG_ERROR("Failed to open file {} for writing", file_name);
            return;
        }

        output_file.write(reinterpret_cast<const char *>(file_header.data()), dds_descriptor->headerSize);

        if (log_texture_export)
            LOG_DEBUG("Exporting texture {} ({}x{})", file_name, width, height);

        export_dds_swap_rb = dds_swap_rb(dxgi_format);
    }

    exporting_texture = true;
}

void TextureCache::export_texture_impl(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height, uint32_t mip_index, const void *pixels, int face, uint32_t pixels_per_stride) {
    if (!exporting_texture)
        return;

    if (save_as_png && (mip_index != 0 || face != 0))
        // png does not support mipmap / cubemap
        return;

    SceGxmTexture &gxm_texture = current_info->texture;
    uint32_t nb_comp = gxm::get_num_components(base_format);
    bool alpha_is_1 = static_cast<bool>(gxm_texture.swizzle_format & 0b100);
    bool alpha_is_first = static_cast<bool>(gxm_texture.swizzle_format & 0b010);
    bool swap_rb = static_cast<bool>(gxm_texture.swizzle_format & 0b001);

    if (!save_as_png && export_dds_swap_rb)
        swap_rb = !swap_rb;

    const uint32_t nb_pixels = pixels_per_stride * height;
    const uint32_t texture_size = (nb_pixels * gxm::bits_per_pixel(base_format)) / 8;

    // first step: remove the swizzle for textures with 3 or 4 components
    std::vector<uint8_t> data_unswizzled;
    if (nb_comp >= 3 && (alpha_is_first || swap_rb)) {
        data_unswizzled.resize(texture_size);

        switch (base_format) {
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8: {
            // case not handled by our reverse function
            const uint8_t *src_pixels = static_cast<const uint8_t *>(pixels);
            for (uint32_t i = 0; i < nb_pixels; i++) {
                data_unswizzled[3 * i + 0] = src_pixels[3 * i + 2];
                data_unswizzled[3 * i + 1] = src_pixels[3 * i + 1];
                data_unswizzled[3 * i + 2] = src_pixels[3 * i + 0];
            }
            break;
        }
        case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
            reverse_comp3_order<uint32_t, 9, 9, 9>(pixels, data_unswizzled.data(), nb_pixels);
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
            reverse_comp3_order<uint16_t, 5, 6, 5>(pixels, data_unswizzled.data(), nb_pixels);
            break;

        // 4 components
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
            apply_swizzle_4<uint32_t, 8, 8, 8, 8>(pixels, data_unswizzled.data(), nb_pixels, alpha_is_1, swap_rb);
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
        case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
            apply_swizzle_4<uint64_t, 16, 16, 16, 16>(pixels, data_unswizzled.data(), nb_pixels, alpha_is_1, swap_rb);
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
            apply_swizzle_4<uint32_t, 10, 10, 10, 2>(pixels, data_unswizzled.data(), nb_pixels, alpha_is_1, swap_rb);
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
            apply_swizzle_4<uint16_t, 4, 4, 4, 4>(pixels, data_unswizzled.data(), nb_pixels, alpha_is_1, swap_rb);
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
            apply_swizzle_4<uint16_t, 5, 5, 5, 1>(pixels, data_unswizzled.data(), nb_pixels, alpha_is_1, swap_rb);
            break;
        default:
            LOG_ERROR("Unhandled swizzle for texture format 0x{:0X}, please report it to the developers.", fmt::underlying(base_format));
            return;
        }

        pixels = data_unswizzled.data();
    }

    if (save_as_png) {
        // only save the first mip (and no cube map)
        if (mip_index != 0 || face != 0)
            return;

        // convert the texture if necessary
        std::vector<uint8_t> converted_data;
        if (base_format != SCE_GXM_TEXTURE_BASE_FORMAT_U8
            && base_format != SCE_GXM_TEXTURE_BASE_FORMAT_U8U8
            && base_format != SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8
            && base_format != SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8)
            converted_data.resize(pixels_per_stride * align(height, 4) * nb_comp);

        std::array<uint8_t, 4> *data_dst4 = reinterpret_cast<std::array<uint8_t, 4> *>(converted_data.data());
        std::array<uint8_t, 3> *data_dst3 = reinterpret_cast<std::array<uint8_t, 3> *>(converted_data.data());

        // the png write function only accepts u8* textures, so convert everything to it
        switch (base_format) {
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
            // already converted
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
        case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
        case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
            texture::decompress_compressed_texture(base_format, converted_data.data(), pixels, pixels_per_stride, height);
            break;

        case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4: {
            const uint16_t *src = static_cast<const uint16_t *>(pixels);
            for (uint32_t i = 0; i < nb_pixels; i++) {
                uint8_t r = src[i] & 0xF;
                uint8_t g = (src[i] >> 4) & 0xF;
                uint8_t b = (src[i] >> 8) & 0xF;
                uint8_t a = (src[i] >> 12);
                data_dst4[i][0] = (r << 4) | r;
                data_dst4[i][1] = (g << 4) | g;
                data_dst4[i][2] = (b << 4) | b;
                data_dst4[i][3] = (a << 4) | a;
            }
            break;
        }
        case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5: {
            const uint16_t *src = static_cast<const uint16_t *>(pixels);
            for (uint32_t i = 0; i < nb_pixels; i++) {
                uint8_t r = src[i] & 0x1F;
                uint8_t g = (src[i] >> 5) & 0x3F;
                uint8_t b = (src[i] >> 11) & 0x1F;
                data_dst3[i][0] = (r << 3) | (r >> 2);
                data_dst3[i][1] = (g << 2) | (g >> 4);
                data_dst3[i][2] = (b << 3) | (b >> 2);
            }
            break;
        }
        case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5: {
            const uint16_t *src = static_cast<const uint16_t *>(pixels);
            for (uint32_t i = 0; i < nb_pixels; i++) {
                uint8_t r = src[i] & 0x1F;
                uint8_t g = (src[i] >> 5) & 0x1F;
                uint8_t b = (src[i] >> 10) & 0x1F;
                bool a = static_cast<bool>(src[i] & (1 << 15));
                data_dst4[i][0] = (r << 3) | (r >> 2);
                data_dst4[i][1] = (g << 2) | (g >> 4);
                data_dst4[i][2] = (b << 3) | (b >> 2);
                data_dst4[i][3] = a ? 255 : 0;
            }
            break;
        }

        case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10: {
            const uint32_t *src = static_cast<const uint32_t *>(pixels);
            for (uint32_t i = 0; i < nb_pixels; i++) {
                uint32_t r = src[i] & 0x3FF;
                uint32_t g = (src[i] >> 10) & 0x3FF;
                uint32_t b = (src[i] >> 20) & 0x3FF;
                uint8_t a = (src[i] >> 30);
                data_dst4[i][0] = r >> 2;
                data_dst4[i][1] = g >> 2;
                data_dst4[i][2] = b >> 2;
                data_dst4[i][3] = a | (a << 2) | (a << 4) | (a << 6);
            }
            break;
        }

        case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16: {
            const uint16_t *src = static_cast<const uint16_t *>(pixels);
            for (uint32_t i = 0; i < nb_pixels * nb_comp; i++)
                converted_data[i] = src[i] >> 8;
            break;
        }
        case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
        case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
        case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16: {
            const uint16_t *src = static_cast<const uint16_t *>(pixels);
            for (uint32_t i = 0; i < nb_pixels * nb_comp; i++)
                converted_data[i] = static_cast<uint8_t>(std::clamp(util::decode_flt16(src[i]) * 255.0f, 0.0f, 255.0f));
            break;
        }
        case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
        case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32: {
            const float *src = static_cast<const float *>(pixels);
            for (uint32_t i = 0; i < nb_pixels * nb_comp; i++)
                converted_data[i] = static_cast<uint8_t>(std::clamp(src[i] * 255.0f + 0.5f, 0.0f, 255.0f));
            break;
        }
        case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32: {
            const uint32_t *src = static_cast<const uint32_t *>(pixels);
            for (uint32_t i = 0; i < nb_pixels * nb_comp; i++)
                converted_data[i] = src[i] >> 24;
            break;
        }
        default:
            LOG_ERROR("Unhandled format for png exportation 0x{:0X}, please report it to the developers.", fmt::underlying(base_format));
            return;
        }

        const uint8_t *data = converted_data.empty() ? static_cast<const uint8_t *>(pixels) : converted_data.data();

        // if the texture has 4 components but the alpha is set to 1 using the swizzle, convert it to a 3-component texture
        std::vector<uint8_t> data_comp3;
        if (nb_comp == 4 && alpha_is_1) {
            data_comp3.resize(nb_pixels * 3);
            for (uint32_t i = 0; i < nb_pixels; i++) {
                data_comp3[i * 3 + 0] = data[i * 4 + 0];
                data_comp3[i * 3 + 1] = data[i * 4 + 1];
                data_comp3[i * 3 + 2] = data[i * 4 + 2];
            }
            data = data_comp3.data();
            nb_comp = 3;
        }

        if (gxm_texture.gamma_mode != 0 && nb_comp >= 3) {
            // we need to convert srgb to linear
            bool cant_overwrite_data = data_unswizzled.empty() && converted_data.empty() && data_comp3.empty();
            if (cant_overwrite_data) {
                converted_data.resize(nb_pixels * nb_comp);
                memcpy(converted_data.data(), data, nb_pixels * nb_comp);
                data = converted_data.data();
            }
            uint8_t *src_pixels = const_cast<uint8_t *>(data);

            auto convert_to_linear = [](uint8_t pixel) {
                // linear = srgb^(2.2)
                return static_cast<uint8_t>(roundf(powf(static_cast<float>(pixel) / 255.0f, 2.2f) * 255.0f));
            };
            if (nb_comp == 3) {
                for (uint32_t i = 0; i < nb_pixels; i++) {
                    src_pixels[i] = convert_to_linear(src_pixels[i]);
                }
            } else {
                // alpha is already linear
                for (uint32_t i = 0; i < nb_pixels; i++) {
                    src_pixels[i * 4 + 0] = convert_to_linear(src_pixels[i * 4 + 0]);
                    src_pixels[i * 4 + 1] = convert_to_linear(src_pixels[i * 4 + 1]);
                    src_pixels[i * 4 + 2] = convert_to_linear(src_pixels[i * 4 + 2]);
                }
            }
        }

        uint64_t hash = current_info->hash;
        const std::string file_name = fmt::format("{:016X}.png", hash);
        fs::path export_name = export_folder / file_name;
        stbi_write_png(fs_utils::path_to_utf8(export_name).c_str(), width, height, nb_comp, data, pixels_per_stride * nb_comp);

        if (log_texture_export)
            LOG_DEBUG("Texture {} ({}x{}) exported", file_name, width, height);

        exported_textures_hash.insert(hash);
        return;
    }

    // export as dds
    std::vector<uint8_t> expanded_data;
    if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8 || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8) {
        // 24bpp textures are not supported nby dds files, convert them to rgba8
        expanded_data.resize(width * height * 4);
        const uint8_t *data = static_cast<const uint8_t *>(pixels);
        for (uint32_t i = 0; i < width * height; i++) {
            expanded_data[4 * i + 0] = data[3 * i + 0];
            expanded_data[4 * i + 1] = data[3 * i + 1];
            expanded_data[4 * i + 2] = data[3 * i + 2];
            expanded_data[4 * i + 3] = 255;
        }
        pixels = expanded_data.data();
        base_format = (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8) ? SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8 : SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8;
    }
    auto [block_width, block_height] = gxm::get_block_size(base_format);
    width = align(width, block_width);
    height = align(height, block_height);
    if (face > 0)
        face--;
    const size_t file_offset = ddspp::get_offset(*dds_descriptor, mip_index, face);
    output_file.seekp(dds_descriptor->headerSize + file_offset);

    const uint32_t bpp = gxm::bits_per_pixel(base_format);
    uint32_t block_stride_in_bytes = (pixels_per_stride * block_height * bpp) / 8;
    uint32_t block_width_in_bytes = (width * block_height * bpp) / 8;
    uint32_t nb_blocks_y = height / block_height;
    const char *data_loc = static_cast<const char *>(pixels);
    for (uint32_t block_y = 0; block_y < nb_blocks_y; block_y++) {
        output_file.write(data_loc, block_width_in_bytes);
        data_loc += block_stride_in_bytes;
    }
}

void TextureCache::export_done() {
    if (exporting_texture && !save_as_png) {
        output_file.close();
        exported_textures_hash.insert(current_info->hash);
    }

    exporting_texture = false;
}

bool TextureCache::import_configure_texture() {
    uint64_t hash = current_info->hash;
    const std::string file_name = fmt::format("{:016X}.{}", hash, loading_texture.is_dds ? "dds" : "png");
    fs::path import_name = *loading_texture.folder_path / file_name;

    if (!fs::exists(import_name)) {
        LOG_ERROR("Texture {} was listed as available but was not found", file_name);
        return false;
    }

    SceGxmTexture &gxm_texture = current_info->texture;
    const SceGxmTextureBaseFormat format = gxm::get_base_format(gxm::get_format(gxm_texture));
    uint32_t nb_comp = gxm::get_num_components(format);

    const bool is_cube = gxm_texture.texture_type() == SCE_GXM_TEXTURE_CUBE || gxm_texture.texture_type() == SCE_GXM_TEXTURE_CUBE_ARBITRARY;
    if (is_cube && !loading_texture.is_dds) {
        LOG_ERROR("Trying to import cubemap as png {}", file_name);
        return false;
    }

    // with 3-component or 4-component textures with a specific swizzle, upload them as 4 component
    // (rgb8 textures are not that much supported on modern gpus)
    if (nb_comp == 3)
        nb_comp = 4;

    uint32_t width, height;
    uint16_t mipcount = 1;
    SceGxmTextureBaseFormat base_format;
    bool is_srgb = false;
    bool swap_rb = false;
    if (loading_texture.is_dds) {
        if (dds_descriptor == nullptr)
            dds_descriptor = new ddspp::Descriptor;

        auto res = fs_utils::read_data(import_name, imported_texture_raw_data);
        if (!res) {
            LOG_ERROR("Failed to read {}", file_name);
            return false;
        }
        if (imported_texture_raw_data.size() < ddspp::MAX_HEADER_SIZE) {
            imported_texture_raw_data.resize(ddspp::MAX_HEADER_SIZE);
        }

        if (ddspp::decode_header(imported_texture_raw_data.data(), *dds_descriptor) != ddspp::Success) {
            LOG_ERROR("Failed to decode file {} header", file_name);
            return false;
        }

        if ((dds_descriptor->type == ddspp::Cubemap) != is_cube) {
            if (is_cube)
                LOG_ERROR("Texture {} should be a cubemap but is a 2D texture", file_name);
            else
                LOG_ERROR("Texture {} should be a 2D texture but is cubemap", file_name);
            return false;
        }

        width = dds_descriptor->width;
        height = dds_descriptor->height;
        mipcount = dds_descriptor->numMips;
        base_format = dxgi_to_gxm(dds_descriptor->format);
        if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_INVALID) {
            LOG_ERROR("dds format {} used by texture {} is unhandled", fmt::underlying(dds_descriptor->format), file_name);
            return false;
        }
        is_srgb = ddspp::is_srgb(dds_descriptor->format);
        swap_rb = dds_swap_rb(dds_descriptor->format);

        if (texture::is_astc_format(base_format) && !support_astc) {
            LOG_ERROR_ONCE("ASTC textures are not support by this device");
            return false;
        }

        if (gxm::is_bcn_format(base_format) && !support_dxt) {
            LOG_ERROR_ONCE("BCn textures are not supported by this device");
#ifdef __ANDROID__
            // this issue is most likely to happen on android
            SDL_ShowAndroidToast("BCn textures are not supported by this device!", 1, -1, 0, 0);
#endif
            return false;
        }

        imported_texture_decoded = imported_texture_raw_data.data() + dds_descriptor->headerSize;
    } else {
        int nb_channels;
        imported_texture_decoded = stbi_load(fs_utils::path_to_utf8(import_name).c_str(), reinterpret_cast<int *>(&width), reinterpret_cast<int *>(&height), &nb_channels, nb_comp);
        if (imported_texture_decoded == nullptr) {
            LOG_ERROR("Failed to decode {}", file_name);
            return false;
        }

        if (nb_comp >= 3 && nb_channels <= 2) {
            LOG_ERROR("Texture {} has {} channels, expected {}", file_name, nb_channels, nb_comp);
            stbi_image_free(imported_texture_decoded);
            return false;
        }

        if (nb_comp == 1)
            base_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8;
        else if (nb_comp == 2)
            base_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8;
        else
            base_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8;
    }

    if (log_texture_import)
        LOG_DEBUG("Importing texture {} ({}x{})", file_name, width, height);

    if (current_info->is_imported
        && current_info->width == width
        && current_info->height == height
        && current_info->mip_count == 1
        && current_info->format == base_format
        && current_info->is_srgb == is_srgb) {
        // no parameter was changed, no need to reconfigure the texture
        return true;
    }

    current_info->is_imported = true;
    current_info->width = width;
    current_info->height = height;
    current_info->mip_count = mipcount;
    current_info->format = base_format;
    current_info->is_srgb = is_srgb;

    import_configure_impl(base_format, width, height, is_srgb, nb_comp, mipcount, swap_rb);
    return true;
}

void TextureCache::import_upload_texture() {
    if (loading_texture.is_dds) {
        auto [block_width, _] = gxm::get_block_size(current_info->format);
        const uint32_t mipcount = current_info->mip_count;
        const bool is_cube = current_info->texture.texture_type() == SCE_GXM_TEXTURE_CUBE || current_info->texture.texture_type() == SCE_GXM_TEXTURE_CUBE_ARBITRARY;

        // upload each face one by one
        for (uint32_t face = 0; face < (is_cube ? 6 : 1); face++) {
            uint32_t width = current_info->width;
            uint32_t height = current_info->height;
            // upload each mip one by one
            for (uint32_t mip = 0; mip < mipcount; mip++) {
                const uint8_t *mip_data = imported_texture_decoded + ddspp::get_offset(*dds_descriptor, mip, face);
                // dds textures are tightly packed (up to the block size)
                upload_texture_impl(current_info->format, width, height, mip, mip_data, is_cube + face, align(width, block_width));

                // on to the next mip
                width /= 2;
                height /= 2;
            }
        }
    } else {
        // just upload the first mip and we are done (png does not support multiple mips / cubemaps)
        upload_texture_impl(current_info->format, current_info->width, current_info->height, 0, imported_texture_decoded, 0, current_info->width);
    }
}

void TextureCache::import_done() {
    if (loading_texture.is_dds) {
        imported_texture_raw_data.clear();
        imported_texture_decoded = nullptr;
    } else {
        if (imported_texture_decoded) {
            stbi_image_free(imported_texture_decoded);
            imported_texture_decoded = nullptr;
        }
    }
}

void TextureCache::refresh_available_textures() {
    if (import_folder.empty() || export_folder.empty())
        return;

    auto look_through_folder = [&]<typename F>(const fs::path &folder, F on_texture_found) {
        fs::create_directories(folder);

        // iterate through all the files, and list the png/dds inside
        for (const auto &file_entry : fs::recursive_directory_iterator(folder)) {
            const fs::path &file = file_entry.path();
            if (!fs::is_regular_file(file))
                continue;

            uint64_t hash;
            if (!(std::istringstream{ file.filename().string() } >> std::hex >> hash))
                continue;

            if (file.extension() != ".png" && file.extension() != ".dds")
                continue;

            bool is_dds = file.extension() == ".dds";

            on_texture_found(hash, file, is_dds);
        }
    };

    exported_textures_hash.clear();
    if (export_textures) {
        look_through_folder(export_folder, [&](uint64_t hash, const fs::path &file, bool is_dds) {
            // for exported textures, do not look in subfolders
            if (file.parent_path() != export_folder)
                return;

            if (save_as_png == !is_dds)
                exported_textures_hash.insert(hash);
        });

        if (!exported_textures_hash.empty())
            LOG_INFO("Found {} textures already exported", exported_textures_hash.size());
    }

    available_textures_hash.clear();
    if (import_textures) {
        // to reduce memory, reuse the same path for multiple textures in the same folder
        std::map<fs::path, std::shared_ptr<fs::path>> found_folders;

        look_through_folder(import_folder, [&](uint64_t hash, const fs::path &file, bool is_dds) {
            // prioritize dds files
            if (is_dds || available_textures_hash.find(hash) == available_textures_hash.end()) {
                const fs::path parent = file.parent_path();
                auto it = found_folders.find(parent);

                if (it == found_folders.end())
                    it = found_folders.emplace(parent, std::make_shared<fs::path>(parent)).first;

                available_textures_hash[hash] = {
                    is_dds,
                    it->second
                };
            }
        });

        if (!available_textures_hash.empty())
            LOG_INFO("Found {} textures ready to be imported", available_textures_hash.size());
    }
}

static SceGxmTextureBaseFormat dxgi_to_gxm(const ddspp::DXGIFormat format) {
    using namespace ddspp;

    switch (format) {
    case R16G16B16A16_FLOAT:
        return SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16;
    case R16G16B16A16_UNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16;
    case R16G16B16A16_SNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16;
    case R32G32_FLOAT:
        return SCE_GXM_TEXTURE_BASE_FORMAT_F32F32;
    case R10G10B10A2_UNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10;
    case R11G11B10_FLOAT:
        return SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10;
    case R8G8B8A8_UNORM:
    case R8G8B8A8_UNORM_SRGB:
    case B8G8R8A8_UNORM:
    case B8G8R8A8_UNORM_SRGB:
    case B8G8R8X8_UNORM:
    case B8G8R8X8_UNORM_SRGB:
        return SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8;
    case R8G8B8A8_SNORM:
    case R8G8B8A8_SINT:
        return SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8;
    case R16G16_FLOAT:
        return SCE_GXM_TEXTURE_BASE_FORMAT_F16F16;
    case R16G16_UNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_U16U16;
    case R16G16_SNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_S16S16;
    case D32_FLOAT:
    case R32_FLOAT:
        return SCE_GXM_TEXTURE_BASE_FORMAT_F32;
    case R8G8_UNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_U8U8;
    case R8G8_SNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_S8S8;
    case R16_FLOAT:
        return SCE_GXM_TEXTURE_BASE_FORMAT_F16;
    case D16_UNORM:
    case R16_UNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_U16;
    case R16_SNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_S16;
    case R8_UNORM:
    case A8_UNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_U8;
    case R8_SNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_S8;
    case R9G9B9E5_SHAREDEXP:
        return SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9;
    case BC1_UNORM:
    case BC1_UNORM_SRGB:
        return SCE_GXM_TEXTURE_BASE_FORMAT_UBC1;
    case BC2_UNORM:
    case BC2_UNORM_SRGB:
        return SCE_GXM_TEXTURE_BASE_FORMAT_UBC2;
    case BC3_UNORM:
    case BC3_UNORM_SRGB:
        return SCE_GXM_TEXTURE_BASE_FORMAT_UBC3;
    case BC4_UNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_UBC4;
    case BC4_SNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_SBC4;
    case BC5_UNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_UBC5;
    case BC5_SNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_SBC5;
    case BC6H_UF16:
        return SCE_GXM_TEXTURE_BASE_FORMAT_UBC6H;
    case BC6H_SF16:
        return SCE_GXM_TEXTURE_BASE_FORMAT_SBC6H;
    case BC7_UNORM:
    case BC7_UNORM_SRGB:
        return SCE_GXM_TEXTURE_BASE_FORMAT_UBC7;
    case B5G6R5_UNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5;
    case B5G5R5A1_UNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5;
    case B4G4R4A4_UNORM:
        return SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4;

#define ASTC_FMT(b_x, b_y)                \
    case ASTC_##b_x##X##b_y##_UNORM:      \
    case ASTC_##b_x##X##b_y##_UNORM_SRGB: \
        return SCE_GXM_TEXTURE_BASE_FORMAT_ASTC##b_x##x##b_y;

#include "../texture/astc_formats.inc"
#undef ASTC_FMT

    default:
        return SCE_GXM_TEXTURE_BASE_FORMAT_INVALID;
    }
}

static ddspp::DXGIFormat gxm_to_dxgi(const SceGxmTextureBaseFormat format) {
    using namespace ddspp;
    switch (format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
        return R8_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
        return R8_SNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
        return B4G4R4A4_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
        return B5G5R5A1_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
        return B5G6R5_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
        return R8G8_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
        return R8G8_SNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
        return R16_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
        return R16_SNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
        return R16_FLOAT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
    // all textures being converted to U8
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
        return R8G8B8A8_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
        return R8G8B8A8_SNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
        return R10G10B10A2_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
        return R16G16_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
        return R16G16_SNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
        return R16G16_FLOAT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
        return R32_FLOAT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
        return R32_UINT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
        return R32_SINT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
        return R9G9B9E5_SHAREDEXP;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
        return R16G16B16A16_FLOAT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
        return R16G16B16A16_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
        return R16G16B16A16_SNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
        return R32G32_FLOAT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
        return R32G32_UINT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        return BC1_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
        return BC2_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        return BC3_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
        return BC4_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        return BC4_SNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
        return BC5_UNORM;
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        return BC5_SNORM;

    default:
        return UNKNOWN;
    }
}

static ddspp::DXGIFormat dxgi_apply_srgb(const ddspp::DXGIFormat format) {
    // we are only interested in format we can get with dxgi_to_gxm
    using namespace ddspp;
    switch (format) {
    case R8G8B8A8_UNORM:
        return R8G8B8A8_UNORM_SRGB;
    case B8G8R8A8_UNORM:
        return B8G8R8A8_UNORM_SRGB;
    case BC1_UNORM:
        return BC1_UNORM_SRGB;
    case BC2_UNORM:
        return BC2_UNORM_SRGB;
    case BC3_UNORM:
        return BC3_UNORM_SRGB;
    case BC7_UNORM:
        return BC7_UNORM_SRGB;

#define ASTC_FMT(b_x, b_y)           \
    case ASTC_##b_x##X##b_y##_UNORM: \
        return ASTC_##b_x##X##b_y##_UNORM_SRGB;

#include "../texture/astc_formats.inc"
#undef ASTC_FMT

    default:
        return format;
    }
}

static bool allowed_png_textures(const SceGxmTextureBaseFormat format) {
    switch (format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        return false;
    default:
        return true;
    }
}

static bool allowed_dds_textures(const SceGxmTextureBaseFormat format) {
    switch (format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
        return false;
    default:
        return true;
    }
}

static bool dds_swap_rb(const ddspp::DXGIFormat format) {
    using namespace ddspp;
    switch (format) {
    case B8G8R8A8_UNORM:
    case B8G8R8X8_UNORM:
    case B8G8R8A8_UNORM_SRGB:
    case B8G8R8X8_UNORM_SRGB:
    case B4G4R4A4_UNORM:
    case B5G5R5A1_UNORM:
    case B5G6R5_UNORM:
        return true;
    default:
        return false;
    }
}

template <typename T, size_t size1, size_t size2, size_t size3, size_t size4>
static void apply_swizzle_4(const void *src, void *dst, uint32_t nb_pixels, bool reverse, bool swap_rb) {
    static_assert(sizeof(T) * 8 == size1 + size2 + size3 + size4);
    static_assert(size1 == size3);
    constexpr T one = 1;
    // these masks are assume the components are in reverse order
    constexpr T maskr1 = ((one << size1) - 1) << (size2 + size3 + size4);
    constexpr T maskr2 = ((one << size2) - 1) << (size3 + size4);
    constexpr T maskr3 = ((one << size3) - 1) << size4;
    constexpr T maskr4 = ((one << size4) - 1);

    // used for swapping r and b
    constexpr T mask_rb = ((one << size1) - 1) | (((one << size3) - 1) << (size1 + size2));

    const T *src_pixels = static_cast<const T *>(src);
    T *dst_pixels = static_cast<T *>(dst);
    for (uint32_t i = 0; i < nb_pixels; i++) {
        // go from rgba to abgr
        // we must take into account that components can have different sizes
        T pixel = src_pixels[i];
        if (reverse) {
            // components are in the reverse order of what we expect
            T r = (pixel & maskr1) >> (size2 + size3 + size4);
            T g = ((pixel & maskr2) >> (size3 + size4)) << size1;
            T b = ((pixel & maskr3) >> size4) << (size1 + size2);
            T a = (pixel & maskr4) << (size1 + size2 + size3);
            pixel = r | g | b | a;
        }
        if (swap_rb) {
            T rb_part = pixel & mask_rb;
            // swap r and b
            rb_part = (rb_part << (size1 + size2)) | (rb_part >> (size1 + size2));
            pixel = (pixel & ~mask_rb) | (rb_part & mask_rb);
        }
        dst_pixels[i] = pixel;
    }
}

// same as reverse_comp3_order but for textures with only 3 components
template <typename T, size_t size1, size_t size2, size_t size3>
static void reverse_comp3_order(const void *src, void *dst, uint32_t nb_pixels) {
    static_assert(size1 == size3);

    constexpr size_t rgb_size = size1 + size2 + size3;
    // this happens only for shared exponent textures
    constexpr bool has_leftover = (rgb_size < sizeof(T) * 8);
    constexpr uint32_t mask_g = ((1 << size2) - 1) << size1
        | (has_leftover ? ~((1 << rgb_size) - 1) : 0);

    const T *src_pixels = static_cast<const T *>(src);
    T *dst_pixels = static_cast<T *>(dst);
    for (uint32_t i = 0; i < nb_pixels; i++) {
        // go from rgb to bgr
        T pixel = src_pixels[i];
        T rb_part = pixel & ~mask_g;
        pixel = (rb_part << (size1 + size2)) | (pixel & mask_g) | (rb_part >> (size1 + size2));
        dst_pixels[i] = pixel;
    }
}

} // namespace renderer
