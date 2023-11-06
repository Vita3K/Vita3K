// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

/*
float32 to float16 conversion
we can have 3 cases
1 program compiled with AVX2 or F16C flags  - use fast conversion
2 program compiled without AVX2 or F16C flags:
2a we can include and use F16C intrinsic without AVX2 or F16C flags - autodetect and use fast or basic conversion depends of runtime cpu
2b we can't include and use F16C intrinsic - use basic conversion
msvc allow to include any intrinsic independent of architecture flags, other compilers disallow this
*/

#include <util/log.h>

#include <cstdint>

#if (defined(__AVX__) && defined(__F16C__)) || defined(__AVX2__) || (defined(_MSC_VER) && !defined(__clang__))
#include <algorithm>
#include <immintrin.h>
void float_to_half_AVX_F16C(const float *src, uint16_t *dest, const int total) {
    int left = total;

    while (left >= 8) {
        __m256 float_vector = _mm256_loadu_ps(src);
        __m128i half_vector = _mm256_cvtps_ph(float_vector, 0);
        _mm_storeu_si128((__m128i *)dest, half_vector);

        left -= 8;
        src += 8;
        dest += 8;
    }

    if (left > 0) {
        alignas(32) float data[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        std::copy_n(src, left, data);
        __m256 float_vector = _mm256_load_ps(data);
        __m128i half_vector = _mm256_cvtps_ph(float_vector, 0);
        _mm_store_si128(reinterpret_cast<__m128i *>(data), half_vector);
        std::copy_n(reinterpret_cast<uint16_t *>(data), left, dest);
    }
}
#endif

#if (defined(__AVX__) && defined(__F16C__)) || defined(__AVX2__)
// forced use AVX+F16C instruction set
// AVX2 checked intentionally cause MSVC does not have __F16C__ macros
// and checking AVX is not enough for some CPU architectures (Intel Sandy bridge)
void float_to_half(const float *src, uint16_t *dest, const int total) {
    float_to_half_AVX_F16C(src, dest, total);
}
#elif defined(__aarch64__)
#include <arm_neon.h>
void float_to_half(const float *src, uint16_t *dest, const int total) {
    int left = total;
    while (left >= 4) {
        float32x4_t floatx4 = vld1q_f32(src);
        float16x4_t halfx4 = vcvt_f16_f32(floatx4);
        vst1_f16(reinterpret_cast<float16_t *>(dest), halfx4);
        left -= 4;
        src += 4;
        dest += 4;
    }

    if (left > 0) {
        float data[4] = { 0.0, 0.0, 0.0, 0.0 };
        std::copy_n(src, left, data);
        float32x4_t floatx4 = vld1q_f32(data);
        float16x4_t halfx4 = vcvt_f16_f32(floatx4);
        vst1_f16(reinterpret_cast<float16_t *>(data), halfx4);
        std::copy_n(reinterpret_cast<uint16_t *>(data), left, dest);
    }
}
#else
#include <util/float_to_half.h>
void float_to_half_basic(const float *src, uint16_t *dest, const int total) {
    for (int i = 0; i < total; i++) {
        dest[i] = util::encode_flt16(src[i]);
    }
}
#if (defined(_MSC_VER) && !defined(__clang__))
// check and use AVX+F16C instruction set if possible

// use function variable as imitation of self-modifying code.
// on first use we check processor features and set appropriate realisation, later we immediately use appropriate realisation
void float_to_half_init(const float *src, uint16_t *dest, const int total);

void (*float_to_half_var)(const float *src, uint16_t *dest, const int total) = float_to_half_init;

#include <util/instrset_detect.h>
void float_to_half_init(const float *src, uint16_t *dest, const int total) {
    if (util::instrset::hasF16C()) {
        float_to_half_var = float_to_half_AVX_F16C;
        LOG_INFO("AVX+F16C instruction set is supported. Using fast f32 to f16 conversion");
    } else {
        float_to_half_var = float_to_half_basic;
        LOG_INFO("AVX+F16C instruction set is not supported. Using basic f32 to f16 conversion");
    }
    (*float_to_half_var)(src, dest, total);
}

void float_to_half(const float *src, uint16_t *dest, const int total) {
    (*float_to_half_var)(src, dest, total);
}
#else
void float_to_half(const float *src, uint16_t *dest, const int total) {
    float_to_half_basic(src, dest, total);
}
#endif
#endif
