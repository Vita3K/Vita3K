#pragma once

#include <cstdint>
#include <vector>

namespace ngs {
    enum class BussType {
        BUSS_MASTER = 0,
        BUSS_COMPRESSOR = 1,
        BUSS_SIDE_CHAIN_COMPRESSOR = 2,
        BUSS_DELAY = 3,
        BUSS_DISTORTION = 4,
        BUSS_ENVELOPE = 5,
        BUSS_EQUALIZATION = 6,
        BUSS_MIXER = 7,
        BUSS_PAUSER = 8,
        BUSS_PITCH_SHIFT = 9,
        BUSS_REVERB = 10,
        BUSS_SAS_EMULATION = 11,
        BUSS_SIMPLE = 12,
        BUSS_ATRAC9 = 13,
        BUSS_SIMPLE_ATRAC9 = 14,
        BUSS_SCREAM = 15,
        BUSS_SCREAM_ATRAC9 = 16,
        BUSS_NORMAL_PLAYER = 17
    };

    using PCMChannelBuf = std::vector<std::uint8_t>;
}