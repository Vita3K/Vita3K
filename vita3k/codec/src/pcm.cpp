// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <cassert>
#include <codec/state.h>

extern "C" {
#include <libswresample/swresample.h>
}

#include <util/log.h>

/* PSVita ADPCM table */
static const int16_t hevag_coefs[128][4] = {
    { 0, 0, 0, 0 },
    { 7680, 0, 0, 0 },
    { 14720, -6656, 0, 0 },
    { 12544, -7040, 0, 0 },
    { 15616, -7680, 0, 0 },
    { 14731, -7059, 0, 0 },
    { 14507, -7366, 0, 0 },
    { 13920, -7522, 0, 0 },
    { 13133, -7680, 0, 0 },
    { 12028, -7680, 0, 0 },
    { 10764, -7680, 0, 0 },
    { 9359, -7680, 0, 0 },
    { 7832, -7680, 0, 0 },
    { 6201, -7680, 0, 0 },
    { 4488, -7680, 0, 0 },
    { 2717, -7680, 0, 0 },
    { 910, -7680, 0, 0 },
    { -910, -7680, 0, 0 },
    { -2717, -7680, 0, 0 },
    { -4488, -7680, 0, 0 },
    { -6201, -7680, 0, 0 },
    { -7832, -7680, 0, 0 },
    { -9359, -7680, 0, 0 },
    { -10764, -7680, 0, 0 },
    { -12028, -7680, 0, 0 },
    { -13133, -7680, 0, 0 },
    { -13920, -7522, 0, 0 },
    { -14507, -7366, 0, 0 },
    { -14731, -7059, 0, 0 },
    { 5376, -9216, 3328, -3072 },
    { -6400, -7168, -3328, -2304 },
    { -10496, -7424, -3584, -1024 },
    { -167, -2722, -494, -541 },
    { -7430, -2221, -2298, 424 },
    { -8001, -3166, -2814, 289 },
    { 6018, -4750, 2649, -1298 },
    { 3798, -6946, 3875, -1216 },
    { -8237, -2596, -2071, 227 },
    { 9199, 1982, -1382, -2316 },
    { 13021, -3044, -3792, 1267 },
    { 13112, -4487, -2250, 1665 },
    { -1668, -3744, -6456, 840 },
    { 7819, -4328, 2111, -506 },
    { 9571, -1336, -757, 487 },
    { 10032, -2562, 300, 199 },
    { -4745, -4122, -5486, -1493 },
    { -5896, 2378, -4787, -6947 },
    { -1193, -9117, -1237, -3114 },
    { 2783, -7108, -1575, -1447 },
    { -7334, -2062, -2212, 446 },
    { 6127, -2577, -315, -18 },
    { 9457, -1858, 102, 258 },
    { 7876, -4483, 2126, -538 },
    { -7172, -1795, -2069, 482 },
    { -7358, -2102, -2233, 440 },
    { -9170, -3509, -2674, -391 },
    { -2638, -2647, -1929, -1637 },
    { 1873, 9183, 1860, -5746 },
    { 9214, 1859, -1124, -2427 },
    { 13204, -3012, -4139, 1370 },
    { 12437, -4792, -256, 622 },
    { -2653, -1144, -3182, -6878 },
    { 9331, -1048, -828, 507 },
    { 1642, -620, -946, -4229 },
    { 4246, -7585, -533, -2259 },
    { -8988, -3891, -2807, 44 },
    { -2562, -2735, -1730, -1899 },
    { 3182, -483, -714, -1421 },
    { 7937, -3844, 2821, -1019 },
    { 10069, -2609, 314, 195 },
    { 8400, -3297, 1551, -155 },
    { -8529, -2775, -2432, -336 },
    { 9477, -1882, 108, 256 },
    { 75, -2241, -298, -6937 },
    { -9143, -4160, -2963, 5 },
    { -7270, -1958, -2156, 460 },
    { -2740, 3745, 5936, -1089 },
    { 8993, 1948, -683, -2704 },
    { 13101, -2835, -3854, 1055 },
    { 9543, -1961, 130, 250 },
    { 5272, -4270, 3124, -3157 },
    { -7696, -3383, -2907, -456 },
    { 7309, 2523, 434, -2461 },
    { 10275, -2867, 391, 172 },
    { 10940, -3721, 665, 97 },
    { 24, -310, -1262, 320 },
    { -8122, -2411, -2311, -271 },
    { -8511, -3067, -2337, 163 },
    { 326, -3846, 419, -933 },
    { 8895, 2194, -541, -2880 },
    { 12073, -1876, -2017, -601 },
    { 8729, -3423, 1674, -169 },
    { 12950, -3847, -3007, 1946 },
    { 10038, -2570, 302, 198 },
    { 9385, -2757, 1008, 41 },
    { -4720, -5006, -2852, -1161 },
    { 7869, -4326, 2135, -501 },
    { 2450, -8597, 1299, -2780 },
    { 10192, -2763, 360, 181 },
    { 11313, -4213, 833, 53 },
    { 10154, -2716, 345, 185 },
    { 9638, -1417, -737, 482 },
    { 3854, -4554, 2843, -3397 },
    { 6699, -5659, 2249, -1074 },
    { 11082, -3908, 728, 80 },
    { -1026, -9810, -805, -3462 },
    { 10396, -3746, 1367, -96 },
    { 10287, 988, -1915, -1437 },
    { 7953, 3878, -764, -3263 },
    { 12689, -3375, -3354, 2079 },
    { 6641, 3166, 231, -2089 },
    { -2348, -7354, -1944, -4122 },
    { 9290, -4039, 1885, -246 },
    { 4633, -6403, 1748, -1619 },
    { 11247, -4125, 802, 61 },
    { 9807, -2284, 219, 222 },
    { 9736, -1536, -706, 473 },
    { 8440, -3436, 1562, -176 },
    { 9307, -1021, -835, 509 },
    { 1698, -9025, 688, -3037 },
    { 10214, -2791, 368, 179 },
    { 8390, 3248, -758, -2989 },
    { 7201, 3316, 46, -2614 },
    { -88, -7809, -538, -4571 },
    { 6193, -5189, 2760, -1245 },
    { 12325, -1290, -3284, 253 },
    { 13064, -4075, -2824, 1877 },
    { 5333, 2999, 775, -1132 }
};

static int nibble_lookup[16] = { 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1 };

/**
 * Sony's HEVAG (High Efficiency VAG) ADPCM, used in PSVita games (hardware decoded).
 * Evolution of the regular VAG (same flags and frames), uses 4 history samples and a bigger table.
 *
 * Original research and algorithm by id-daemon / daemon1.
 * Implementation used from vgmstream project, code by bnnm and korenkonder.
 */
bool PCMDecoderState::send(const uint8_t *data, uint32_t size) {
    const std::uint8_t *source_transformed = data;
    std::uint32_t produced_samples = 0;

    std::vector<std::int16_t> transformed;

    if (he_adpcm) {
        const std::uint32_t bytes_per_frame = 0x10;
        const std::uint32_t samples_per_frame = (bytes_per_frame - 2) * 2;

        if (size % bytes_per_frame != 0) {
            LOG_ERROR("Unaligned HEADPCM frame size");
            return false;
        }

        for (std::uint32_t i = 0; i < size / bytes_per_frame; i++) {
            int32_t hist1 = adpcm_history1;
            int32_t hist2 = adpcm_history2;
            int32_t hist3 = adpcm_history3;
            int32_t hist4 = adpcm_history4;

            const std::uint8_t *frame = reinterpret_cast<const std::uint8_t *>(data + bytes_per_frame * i);

            std::uint8_t coef_index = (frame[0] >> 4) & 0xf;
            std::uint8_t shift_factor = (frame[0] >> 0) & 0xf;
            coef_index = ((frame[1] >> 0) & 0xf0) | coef_index;

            const std::uint8_t flag = (frame[1] >> 0) & 0xf;

            if ((coef_index > 127) || (shift_factor > 12)) {
                LOG_WARN("HEVAG: in+correct coefs/shift at frame {}", i);
            }

            if (coef_index > 127)
                coef_index = 127; /* ? */

            if (shift_factor > 12)
                shift_factor = 9; /* ? */

            shift_factor = 20 - shift_factor;

            for (std::uint32_t i = 0; i < samples_per_frame; i++) {
                int32_t sample = 0;

                if (flag < 0x07) { /* with flag 0x07 decoded sample must be 0 */
                    uint8_t nibbles = frame[0x02 + i / 2];

                    sample = (i & 1 ? /* low nibble first */
                                     nibble_lookup[nibbles >> 4]
                                    : nibble_lookup[nibbles & 0xF])
                        << shift_factor; /*scale*/
                    sample = ((hist1 * hevag_coefs[coef_index][0] + hist2 * hevag_coefs[coef_index][1] + hist3 * hevag_coefs[coef_index][2] + hist4 * hevag_coefs[coef_index][3]) >> 5) + sample;
                    sample >>= 8;
                }

                transformed.push_back(static_cast<std::int16_t>(std::clamp(sample, -32768, 32767)));

                hist4 = hist3;
                hist3 = hist2;
                hist2 = hist1;
                hist1 = sample;
            }

            adpcm_history1 = hist1;
            adpcm_history2 = hist2;
            adpcm_history3 = hist3;
            adpcm_history4 = hist4;
        }

        source_transformed = reinterpret_cast<std::uint8_t *>(transformed.data());
        produced_samples = transformed.size() / source_channels;
    } else {
        produced_samples = size / sizeof(std::int16_t) / source_channels;
    }

    // Try to resample if neccessary
    const int source_channel_type = (source_channels == 2) ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
    const int dest_channel_type = AV_CH_LAYOUT_STEREO;

    SwrContext *swr = swr_alloc_set_opts(nullptr,
        dest_channel_type, AV_SAMPLE_FMT_FLT, static_cast<int>(dest_frequency),
        source_channel_type, AV_SAMPLE_FMT_S16, static_cast<int>(source_frequency),
        0, nullptr);

    swr_init(swr);
    const int dest_count = swr_get_out_samples(swr, produced_samples);

    final_result.resize(sizeof(float) * dest_count * 2);
    if (dest_count == 0) {
        return true;
    }

    std::uint8_t *dest_data = &final_result[0];

    const int result = swr_convert(swr, &dest_data, dest_count, &source_transformed, produced_samples);
    swr_free(&swr);
    assert(result > 0);
    return true;
}

bool PCMDecoderState::receive(uint8_t *data, DecoderSize *size) {
    if (data) {
        std::memcpy(data, final_result.data(), final_result.size());
    }

    if (size) {
        size->samples = static_cast<std::uint32_t>(final_result.size() / sizeof(float));
    }

    return true;
}

PCMDecoderState::PCMDecoderState(const float dest_frequency)
    : dest_frequency(dest_frequency)
    , he_adpcm(false)
    , source_channels(2)
    , source_frequency(48000.0f)
    , adpcm_history1(0)
    , adpcm_history2(0)
    , adpcm_history3(0)
    , adpcm_history4(0) {
}
