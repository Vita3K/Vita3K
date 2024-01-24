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

/*
float32 to float16 conversion
we can have 2 cases
1 program compiled for aarch64: Use the native __fp16 type
2 autodetect and use fast or basic conversion depends of runtime cpu
*/

#include <util/log.h>

#include <cstdint>

#if defined(__aarch64__)
#include <arm_neon.h>
void float_to_half(const float *src, uint16_t *dest, const int total) {
    // use the native type __fp16
    __fp16 *dest_fp = reinterpret_cast<__fp16 *>(dest);
    for (int i = 0; i < total; i++) {
        dest_fp[i] = static_cast<__fp16>(src[i]);
    }
}
#else
#if defined(__GNUC__) || defined(__clang__)
#define TARGET_F16C __attribute__((__target__("f16c")))
#include <immintrin.h>
#elif defined(_MSC_VER)
#define TARGET_F16C
#include <intrin.h>
#else
#error "Compiler is not supported"
#endif

static void TARGET_F16C float_to_half_AVX_F16C(const float *src, uint16_t *dest, const int total) {
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

#include <util/float_to_half.h>
static void float_to_half_basic(const float *src, uint16_t *dest, const int total) {
    for (int i = 0; i < total; i++) {
        dest[i] = util::encode_flt16(src[i]);
    }
}

// check and use AVX+F16C instruction set if possible

// use function variable as imitation of self-modifying code.
// on first use we check processor features and set appropriate realization, later we immediately use appropriate realization.
static void float_to_half_init(const float *src, uint16_t *dest, const int total);

static void (*float_to_half_var)(const float *src, uint16_t *dest, const int total) = float_to_half_init;

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
#endif
