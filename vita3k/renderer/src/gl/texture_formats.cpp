#include <renderer/gl/functions.h>

#include <gxm/functions.h>
#include <util/log.h>

namespace renderer::gl {
namespace texture {

// SceGxmTextureSwizzle1Mode
static const GLint swizzle_r[4] = { GL_RED, GL_ZERO, GL_ZERO, GL_ONE };
static const GLint swizzle_000r[4] = { GL_RED, GL_ZERO, GL_ZERO, GL_ZERO };
static const GLint swizzle_111r[4] = { GL_RED, GL_ONE, GL_ONE, GL_ONE };
static const GLint swizzle_rrrr[4] = { GL_RED, GL_RED, GL_RED, GL_RED };
static const GLint swizzle_0rrr[4] = { GL_RED, GL_RED, GL_RED, GL_ZERO };
static const GLint swizzle_1rrr[4] = { GL_RED, GL_RED, GL_RED, GL_ONE };
static const GLint swizzle_r000[4] = { GL_ZERO, GL_ZERO, GL_ZERO, GL_RED };
static const GLint swizzle_r111[4] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };

// SceGxmTextureSwizzle2Mode
static const GLint swizzle_gr[4] = { GL_RED, GL_GREEN, GL_ZERO, GL_ONE };
static const GLint swizzle_00gr[4] = { GL_RED, GL_GREEN, GL_ZERO, GL_ZERO };
static const GLint swizzle_grrr[4] = { GL_RED, GL_RED, GL_RED, GL_GREEN };
static const GLint swizzle_rggg[4] = { GL_GREEN, GL_GREEN, GL_GREEN, GL_RED };
static const GLint swizzle_grgr[4] = { GL_RED, GL_GREEN, GL_RED, GL_GREEN };
static const GLint swizzle_00rg[4] = { GL_GREEN, GL_RED, GL_ZERO, GL_ZERO };

// SceGxmTextureSwizzle2ModeAlt
static const GLint swizzle_sd[4] = { GL_RED, GL_GREEN, GL_ZERO, GL_ONE };
static const GLint swizzle_ds[4] = { GL_GREEN, GL_RED, GL_ZERO, GL_ONE };

// SceGxmTextureSwizzle3Mode
static const GLint swizzle_rgb[4] = { GL_BLUE, GL_GREEN, GL_RED, GL_ONE };
static const GLint swizzle_bgr[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ONE };

// SceGxmTextureSwizzle4Mode
static const GLint swizzle_abgr[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
static const GLint swizzle_argb[4] = { GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA };
static const GLint swizzle_rgba[4] = { GL_ALPHA, GL_BLUE, GL_GREEN, GL_RED };
static const GLint swizzle_bgra[4] = { GL_ALPHA, GL_RED, GL_GREEN, GL_BLUE };
static const GLint swizzle_1bgr[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ONE };
static const GLint swizzle_1rgb[4] = { GL_BLUE, GL_GREEN, GL_RED, GL_ONE };
static const GLint swizzle_rgb1[4] = { GL_ONE, GL_BLUE, GL_GREEN, GL_RED };
static const GLint swizzle_bgr1[4] = { GL_ONE, GL_RED, GL_GREEN, GL_BLUE };

// SceGxmTextureSwizzleYUV420Mode
// TODO I don't know what these should be.
// NOTE: We'll probably need an intermediate shader pass to translate them to RGB.
static const GLint swizzle_yuv_csc0[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
static const GLint swizzle_yvu_csc0[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
static const GLint swizzle_yuv_csc1[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
static const GLint swizzle_yvu_csc1[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };

// SceGxmTextureSwizzleYUV422Mode
// TODO I don't know what these should be.
// NOTE: We'll probably need an intermediate shader pass to translate them to RGB.
static const GLint swizzle_yuyv_csc0[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
static const GLint swizzle_yvyu_csc0[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
static const GLint swizzle_uyvy_csc0[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
static const GLint swizzle_vyuy_csc0[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
static const GLint swizzle_yuyv_csc1[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
static const GLint swizzle_yvyu_csc1[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
static const GLint swizzle_uyvy_csc1[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
static const GLint swizzle_vyuy_csc1[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };

static const GLint *translate_swizzle(SceGxmTextureSwizzle1Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE1_R:
        return swizzle_r;
    case SCE_GXM_TEXTURE_SWIZZLE1_000R:
        return swizzle_000r;
    case SCE_GXM_TEXTURE_SWIZZLE1_111R:
        return swizzle_111r;
    case SCE_GXM_TEXTURE_SWIZZLE1_RRRR:
        return swizzle_rrrr;
    case SCE_GXM_TEXTURE_SWIZZLE1_0RRR:
        return swizzle_0rrr;
    case SCE_GXM_TEXTURE_SWIZZLE1_1RRR:
        return swizzle_1rrr;
    case SCE_GXM_TEXTURE_SWIZZLE1_R000:
        return swizzle_r000;
    case SCE_GXM_TEXTURE_SWIZZLE1_R111:
        return swizzle_r111;
    }

    return swizzle_r;
}

static const GLint *translate_swizzle(SceGxmTextureSwizzle2Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE2_GR:
        return swizzle_gr;
    case SCE_GXM_TEXTURE_SWIZZLE2_00GR:
        return swizzle_00gr;
    case SCE_GXM_TEXTURE_SWIZZLE2_GRRR:
        return swizzle_grrr;
    case SCE_GXM_TEXTURE_SWIZZLE2_RGGG:
        return swizzle_rggg;
    case SCE_GXM_TEXTURE_SWIZZLE2_GRGR:
        return swizzle_grgr;
    case SCE_GXM_TEXTURE_SWIZZLE2_00RG:
        return swizzle_00rg;
    }

    return swizzle_gr;
}

static const GLint *translate_swizzle(SceGxmTextureSwizzle2ModeAlt mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE2_SD:
        return swizzle_sd;
    case SCE_GXM_TEXTURE_SWIZZLE2_DS:
        return swizzle_ds;
    }

    return swizzle_sd;
}

static const GLint *translate_swizzle(SceGxmTextureSwizzle3Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE3_BGR:
        return swizzle_bgr;
    case SCE_GXM_TEXTURE_SWIZZLE3_RGB:
        return swizzle_rgb;
    }

    return swizzle_bgr;
}

static const GLint *translate_swizzle(SceGxmTextureSwizzle4Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE4_ABGR:
        return swizzle_abgr;
    case SCE_GXM_TEXTURE_SWIZZLE4_ARGB:
        return swizzle_argb;
    case SCE_GXM_TEXTURE_SWIZZLE4_RGBA:
        return swizzle_rgba;
    case SCE_GXM_TEXTURE_SWIZZLE4_BGRA:
        return swizzle_bgra;
    case SCE_GXM_TEXTURE_SWIZZLE4_1BGR:
        return swizzle_1bgr;
    case SCE_GXM_TEXTURE_SWIZZLE4_1RGB:
        return swizzle_1rgb;
    case SCE_GXM_TEXTURE_SWIZZLE4_RGB1:
        return swizzle_rgb1;
    case SCE_GXM_TEXTURE_SWIZZLE4_BGR1:
        return swizzle_bgr1;
    }

    return swizzle_abgr;
}

static const GLint *translate_swizzle(SceGxmTextureSwizzleYUV420Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE_YUV_CSC0:
        return swizzle_yuv_csc0;
    case SCE_GXM_TEXTURE_SWIZZLE_YVU_CSC0:
        return swizzle_yvu_csc0;
    case SCE_GXM_TEXTURE_SWIZZLE_YUV_CSC1:
        return swizzle_yuv_csc1;
    case SCE_GXM_TEXTURE_SWIZZLE_YVU_CSC1:
        return swizzle_yvu_csc1;
    }

    return swizzle_yuv_csc0;
}

static const GLint *translate_swizzle(SceGxmTextureSwizzleYUV422Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE_YUYV_CSC0:
        return swizzle_yuyv_csc0;
    case SCE_GXM_TEXTURE_SWIZZLE_YVYU_CSC0:
        return swizzle_yvyu_csc0;
    case SCE_GXM_TEXTURE_SWIZZLE_UYVY_CSC0:
        return swizzle_uyvy_csc0;
    case SCE_GXM_TEXTURE_SWIZZLE_VYUY_CSC0:
        return swizzle_vyuy_csc0;
    case SCE_GXM_TEXTURE_SWIZZLE_YUYV_CSC1:
        return swizzle_yuyv_csc1;
    case SCE_GXM_TEXTURE_SWIZZLE_YVYU_CSC1:
        return swizzle_yvyu_csc1;
    case SCE_GXM_TEXTURE_SWIZZLE_UYVY_CSC1:
        return swizzle_uyvy_csc1;
    case SCE_GXM_TEXTURE_SWIZZLE_VYUY_CSC1:
        return swizzle_vyuy_csc1;
    }

    return swizzle_yuyv_csc0;
}

GLenum translate_internal_format(SceGxmTextureFormat src) {
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(src);
    switch (base_format) {
    // 1 Component.
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
        return GL_RED;

    // 2 components (red-green.)
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
        return GL_RG;

    // 2 components (depth-stencil.)
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
        return GL_DEPTH_STENCIL;

    // 3 components.
    case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
        return GL_RGB;

    // 4 components.
    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
        return GL_RGBA;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
        return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    }
}

GLenum translate_format(SceGxmTextureFormat src) {
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(src);
    switch (base_format) {
    // 1 Component.
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
        return GL_RED;

    // 2 components (red-green.)
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
        return GL_RG;

    // 2 components (depth-stencil.)
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
        return GL_DEPTH_STENCIL;

    // 3 components.
    case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
        return GL_RGB;

    // 4 components.
    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
        return GL_RGBA;

    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
        return GL_RGB;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
        return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    }
}

GLenum translate_type(SceGxmTextureFormat format) {
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
        return GL_UNSIGNED_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
        return GL_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
        return GL_UNSIGNED_SHORT_4_4_4_4_REV;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
        LOG_WARN("Unhandled base format SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2");
        return GL_UNSIGNED_SHORT_4_4_4_4_REV;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
        return GL_UNSIGNED_SHORT_1_5_5_5_REV;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
        return GL_UNSIGNED_SHORT_5_6_5_REV;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
        LOG_WARN("Unhandled base format SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6");
        return GL_UNSIGNED_SHORT_5_6_5_REV;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
        return GL_UNSIGNED_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
        return GL_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
        return GL_UNSIGNED_SHORT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
        return GL_SHORT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
        return GL_HALF_FLOAT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
        return GL_UNSIGNED_INT_8_8_8_8_REV;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
        return GL_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
        return GL_UNSIGNED_INT_2_10_10_10_REV;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
        return GL_UNSIGNED_SHORT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
        return GL_SHORT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
        return GL_HALF_FLOAT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
        return GL_FLOAT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
        LOG_WARN("Unhandled base format SCE_GXM_TEXTURE_BASE_FORMAT_F32M");
        return GL_FLOAT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8:
        LOG_WARN("Unhandled base format SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8");
        return GL_UNSIGNED_INT_8_8_8_8_REV;
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
        LOG_WARN("Unhandled base format SCE_GXM_TEXTURE_BASE_FORMAT_X8U24");
        return GL_UNSIGNED_INT_24_8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
        return GL_UNSIGNED_INT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
        return GL_INT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
        LOG_WARN("Unhandled base format SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9");
        return GL_UNSIGNED_INT_5_9_9_9_REV;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10:
        LOG_WARN("Unhandled base format SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10");
        return GL_UNSIGNED_INT_2_10_10_10_REV;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
        return GL_HALF_FLOAT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
        return GL_UNSIGNED_SHORT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
        return GL_SHORT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
        return GL_FLOAT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
        return GL_UNSIGNED_INT;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
        return GL_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
        return GL_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
        LOG_WARN("Not perfectly handled base format SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP");
        return GL_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
        LOG_WARN("Not perfectly handled base format SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP");
        return GL_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        return GL_UNSIGNED_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
        return GL_UNSIGNED_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        return GL_UNSIGNED_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
        return GL_UNSIGNED_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
        return GL_UNSIGNED_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
        LOG_WARN("Unhandled base format SCE_GXM_TEXTURE_BASE_FORMAT_YUV422");
        return GL_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
        return GL_UNSIGNED_INT_8_8_8_8_REV;
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
        return GL_UNSIGNED_INT_8_8_8_8_REV;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
        return GL_UNSIGNED_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
        return GL_BYTE;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
        LOG_WARN("Unhandled base format SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10");
        return GL_INT_2_10_10_10_REV;
    }

    LOG_WARN("Unhandled base format {}", log_hex(base_format));
    return GL_UNSIGNED_BYTE;
}

const GLint *translate_swizzle(SceGxmTextureFormat fmt) {
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(fmt);
    const uint32_t swizzle = fmt & SCE_GXM_TEXTURE_SWIZZLE_MASK;
    switch (base_format) {
    // 1 Component.
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
        return translate_swizzle(static_cast<SceGxmTextureSwizzle1Mode>(swizzle));

    // 2 components (red-green.)
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
        return translate_swizzle(static_cast<SceGxmTextureSwizzle2Mode>(swizzle));

    // 2 components (depth-stencil.)
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
        return translate_swizzle(static_cast<SceGxmTextureSwizzle2ModeAlt>(swizzle));

    // 3 components.
    case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
        return translate_swizzle(static_cast<SceGxmTextureSwizzle3Mode>(swizzle));

    // 4 components.
    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
        return translate_swizzle(static_cast<SceGxmTextureSwizzle4Mode>(swizzle));

    // YUV420.
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
        return translate_swizzle(static_cast<SceGxmTextureSwizzleYUV420Mode>(swizzle));

    // YUV422.
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
        return translate_swizzle(static_cast<SceGxmTextureSwizzleYUV422Mode>(swizzle));
    }

    LOG_WARN("Invalid base format {}", log_hex(base_format));
    return swizzle_abgr;
}

GLenum translate_wrap_mode(SceGxmTextureAddrMode src) {
    switch (src) {
    case SCE_GXM_TEXTURE_ADDR_REPEAT:
        return GL_REPEAT;
    case SCE_GXM_TEXTURE_ADDR_CLAMP:
        return GL_CLAMP_TO_EDGE;
    case SCE_GXM_TEXTURE_ADDR_MIRROR_CLAMP:
        return GL_CLAMP_TO_EDGE; // FIXME: GL_MIRROR_CLAMP_TO_EDGE is not supported in OpenGL 4.1 core.
    case SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER:
        return GL_REPEAT; // FIXME: Is this correct?
    case SCE_GXM_TEXTURE_ADDR_CLAMP_FULL_BORDER:
        return GL_CLAMP_TO_BORDER;
    case SCE_GXM_TEXTURE_ADDR_CLAMP_IGNORE_BORDER:
        return GL_CLAMP_TO_BORDER; // FIXME: Is this correct?
    case SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER:
        return GL_CLAMP_TO_BORDER; // FIXME: Is this correct?
    default:
        LOG_WARN("Unsupported texture wrap mode translated: {}", log_hex(src));
        return GL_CLAMP_TO_EDGE;
    }
}

GLenum translate_minmag_filter(SceGxmTextureFilter src) {
    switch (src) {
    case SCE_GXM_TEXTURE_FILTER_POINT:
        return GL_NEAREST;
    case SCE_GXM_TEXTURE_FILTER_LINEAR:
        return GL_LINEAR;
    // TODO support mipmap
    case SCE_GXM_TEXTURE_FILTER_MIPMAP_LINEAR:
        return GL_LINEAR;
    case SCE_GXM_TEXTURE_FILTER_MIPMAP_POINT:
        return GL_NEAREST;
    default:
        LOG_WARN("Unsupported texture min/mag filter translated: {}", log_hex(src));
        return GL_LINEAR;
    }
}

} // namespace texture
} // namespace renderer::gl
