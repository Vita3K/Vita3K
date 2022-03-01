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

#include <util/arm.h>
#include <util/bytes.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <codecvt> // std::codecvt_utf8
#include <iostream>
#include <locale> // std::wstring_convert
#include <memory>
#include <set>
#include <sstream>
#include <string>

#include <memory>
#include <stdexcept>

namespace logging {

static const fs::path &LOG_FILE_NAME = "vita3k.log";
static const char *LOG_PATTERN = "%^[%H:%M:%S.%e] |%L| [%!]: %v%$";
std::vector<spdlog::sink_ptr> sinks;

ExitCode init(const Root &root_paths, bool use_stdout) {
    sinks.clear();
    if (use_stdout)
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    if (add_sink(root_paths.get_base_path() / LOG_FILE_NAME) != Success)
        return InitConfigFailed;

    spdlog::set_error_handler([](const std::string &msg) {
        std::cerr << "spdlog error: " << msg << std::endl;
        assert(0);
    });

    return Success;
}

void set_level(spdlog::level::level_enum log_level) {
    spdlog::set_level(log_level);
}

ExitCode add_sink(const fs::path &log_path) {
    try {
#ifdef WIN32
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.generic_path().wstring(), true));
#else
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.generic_path().string(), true));
#endif
    } catch (const spdlog::spdlog_ex &ex) {
        std::cerr << "File log initialization failed: " << ex.what() << std::endl;
        return InitConfigFailed;
    }

#ifdef _MSC_VER
    if (sinks.size() == 2) { // spdlog is being initialized
        sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
    }
#endif

    spdlog::set_default_logger(std::make_shared<spdlog::logger>("vita3k logger", begin(sinks), end(sinks)));
    spdlog::set_pattern(LOG_PATTERN);
    return Success;
}

typedef std::set<std::string> NameSet;
static std::mutex mutex;
static NameSet logged;

int ret_error_impl(const char *name, const char *error_str, std::uint32_t error_val) {
    bool inserted = false;

    {
        const std::lock_guard<std::mutex> lock(mutex);
        inserted = logged.insert(name).second;
    }

    if (inserted) {
        LOG_ERROR("{} returned {} ({})", name, error_str, log_hex(error_val));
    }

    return error_val;
}

} // namespace logging

namespace string_utils {

std::vector<std::string> split_string(const std::string &str, char delimiter) {
    std::stringstream str_stream(str);
    std::string segment;
    std::vector<std::string> seglist;

    const size_t num_segments = std::count_if(str.begin(), str.end(), [&](char c) {
        return c == delimiter;
    }) + (str.empty() ? 1 : 0);

    seglist.reserve(num_segments);

    while (std::getline(str_stream, segment, delimiter)) {
        seglist.push_back(segment);
    }
    return seglist;
}

std::wstring utf_to_wide(const std::string &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(str);
}

std::string wide_to_utf(const std::wstring &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.to_bytes(str);
}

std::string remove_special_chars(std::string str) {
    for (char &c : str) {
        switch (c) {
        case '/':
        case '\\':
        case ':':
        case '?':
        case '"':
        case '<':
        case '>':
        case '|':
        case '*':
            c = ' ';
            break;
        default:
            continue;
        }
    }
    return str;
}

// Based on: https://stackoverflow.com/a/23135441
// Search and replace "in" with "out" in the given string
void replace(std::string &str, const std::string &in, const std::string &out) {
    size_t pos = 0;
    while ((pos = str.find(in, pos)) != std::string::npos) {
        str.replace(pos, in.length(), out);
        pos += out.length();
    }
}

std::basic_string<uint8_t> string_to_byte_array(std::string string) {
    std::basic_string<uint8_t> hex_bytes;

    for (size_t i = 0; i < string.length(); i += 2) {
        uint16_t byte;
        std::string nextbyte = string.substr(i, 2);
        std::istringstream(nextbyte) >> std::hex >> byte;
        hex_bytes.push_back(static_cast<uint8_t>(byte));
    }
    return hex_bytes;
}

#ifdef WIN32
std::string utf16_to_utf8(const std::u16string &str) {
    std::wstring_convert<std::codecvt_utf8_utf16<int16_t>, int16_t> myconv;
    auto p = reinterpret_cast<const int16_t *>(str.data());
    return myconv.to_bytes(p, p + str.size());
}

std::u16string utf8_to_utf16(const std::string &str) {
    static_assert(sizeof(std::wstring::value_type) == sizeof(std::u16string::value_type),
        "std::wstring and std::u16string are expected to have the same character size");

    std::wstring_convert<std::codecvt_utf8_utf16<int16_t>, int16_t> myconv;
    auto p = reinterpret_cast<const char *>(str.data());
    auto a = myconv.from_bytes(p, p + std::strlen(p));
    return std::u16string(a.begin(), a.end());
}
#else
std::string utf16_to_utf8(const std::u16string &str) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> myconv;
    return myconv.to_bytes(str);
}

std::u16string utf8_to_utf16(const std::string &str) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> myconv;
    return myconv.from_bytes(str);
}
#endif

std::string toupper(const std::string &s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c) { return std::toupper(c); });
    return r;
}

std::string tolower(const std::string &s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return r;
}

} // namespace string_utils

template <>
std::uint16_t byte_swap(std::uint16_t val) {
    return (val >> 8) | (val << 8);
}

template <>
std::uint32_t byte_swap(std::uint32_t val) {
    //        AA              BB00                      CC0000                       DD000000
    return (val >> 24) | ((val >> 8) & 0xFF00) | ((val << 8) & 0xFF0000) | ((val << 24) & 0xFF000000);
}

template <>
std::uint64_t byte_swap(std::uint64_t val) {
    val = ((val << 8) & 0xFF00FF00FF00FF00ULL) | ((val >> 8) & 0x00FF00FF00FF00FFULL);
    val = ((val << 16) & 0xFFFF0000FFFF0000ULL) | ((val >> 16) & 0x0000FFFF0000FFFFULL);

    return (val << 32) | (val >> 32);
}

template <>
std::int16_t byte_swap(std::int16_t val) {
    return byte_swap(static_cast<std::uint16_t>(val));
}

template <>
std::int32_t byte_swap(std::int32_t val) {
    return byte_swap(static_cast<std::uint32_t>(val));
}

template <>
std::int64_t byte_swap(std::int64_t val) {
    return byte_swap(static_cast<std::uint64_t>(val));
}

/*
float32 to float16 conversion
we can have 3 cases
1 program compiled with AVX2 or F16C flags  - use fast conversion
2 program compiled without AVX2 or F16C flags:
2a we can include and use F16C intrinsic without AVX2 or F16C flags - autodetect and use fast or basic conversion depends of runtime cpu
2b we can't include and use F16C intrinsic - use basic conversion
msvc allow to include any intrinsic independent of architecture flags, other compilers disallow this
*/
#if (defined(__AVX__) && defined(__F16C__)) || defined(__AVX2__) || defined(_MSC_VER)
#include <immintrin.h>
void float_to_half_AVX_F16C(const float *src, std::uint16_t *dest, const int total) {
    float toconvert[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    int i = 0;

    while (i < total) {
        memcpy(toconvert, src, std::min<int>(8, total - i) * sizeof(float));

        __m256 float_vector = _mm256_load_ps(toconvert);
        __m128i half_vector = _mm256_cvtps_ph(float_vector, 0);
        _mm_storeu_si128((__m128i *)dest, half_vector);

        i += 8;
        src += 8;
        dest += 8;
    }
}
#endif

#if (defined(__AVX__) && defined(__F16C__)) || defined(__AVX2__)
//forced use AVX+F16C instruction set
//AVX2 checked intentionally cause MSVC does not have __F16C__ macros
//and checking AVX is not enough for some CPU architectures (Intel Sandy bridge)
void float_to_half(const float *src, std::uint16_t *dest, const int total) {
    float_to_half_AVX_F16C(src, dest, total);
}
#else
#include <util/float_to_half.h>
void float_to_half_basic(const float *src, std::uint16_t *dest, const int total) {
    for (int i = 0; i < total; i++) {
        dest[i] = util::encode_flt16(src[i]);
    }
}
#if defined(_MSC_VER)
//check and use AVX+F16C instruction set if possible

// use function variable as imitation of self-modifying code.
// on first use we check processor features and set appropriate realisation, later we immediately use appropriate realisation
void float_to_half_init(const float *src, std::uint16_t *dest, const int total);

void (*float_to_half_var)(const float *src, std::uint16_t *dest, const int total) = float_to_half_init;

#include <util/instrset_detect.h>
void float_to_half_init(const float *src, std::uint16_t *dest, const int total) {
    if (util::instrset::hasF16C()) {
        float_to_half_var = float_to_half_AVX_F16C;
        LOG_INFO("AVX+F16C instruction set is supported. Using fast f32 to f16 conversion");
    } else {
        float_to_half_var = float_to_half_basic;
        LOG_INFO("AVX+F16C instruction set is not supported. Using basic f32 to f16 conversion");
    }
    (*float_to_half_var)(src, dest, total);
}

void float_to_half(const float *src, std::uint16_t *dest, const int total) {
    (*float_to_half_var)(src, dest, total);
}
#else
void float_to_half(const float *src, std::uint16_t *dest, const int total) {
    float_to_half_basic(src, dest, total);
}
#endif
#endif

// pent0 found on stackoverflow
// https://stackoverflow.com/questions/4398711/round-to-the-nearest-power-of-two/4398799
std::uint32_t nearest_power_of_two(std::uint32_t num) {
    num--;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    num++;

    return num;
}

// Encode code taken from https://github.com/yifanlu/UVLoader/blob/master/resolve.c

uint32_t encode_arm_inst(uint8_t type, uint32_t immed, uint16_t reg) {
    switch (type) {
    case INSTRUCTION_MOVW:
        // 1110 0011 0000 XXXX YYYY XXXXXXXXXXXX
        // where X is the immediate and Y is the register
        // Upper bits == 0xE30
        return ((uint32_t)0xE30 << 20) | ((uint32_t)(immed & 0xF000) << 4) | (immed & 0xFFF) | (reg << 12);
    case INSTRUCTION_MOVT:
        // 1110 0011 0100 XXXX YYYY XXXXXXXXXXXX
        // where X is the immediate and Y is the register
        // Upper bits == 0xE34
        return ((uint32_t)0xE34 << 20) | ((uint32_t)(immed & 0xF000) << 4) | (immed & 0xFFF) | (reg << 12);
    case INSTRUCTION_SYSCALL:
        // Syscall does not have any immediate value, the number should
        // already be in R12
        return (uint32_t)0xEF000000;
    case INSTRUCTION_BRANCH:
        // 1110 0001 0010 111111111111 0001 YYYY
        // BX Rn has 0xE12FFF1 as top bytes
        return ((uint32_t)0xE12FFF1 << 4) | reg;
    case INSTRUCTION_BLX:
        return ((uint32_t)0x7D << 25) | ((immed & 0x80000000) >> 8) | (immed & 0x7fffff);
    case INSTRUCTION_UNKNOWN:
    default:
        return 0;
    }
}

inline uint32_t encode_thumb_blx(uint32_t immed) {
    const uint32_t S = (immed & 0x80000000) >> 31;
    const uint32_t I1 = (immed & 0x800000) >> 22;
    const uint32_t I2 = (immed & 0x400000) >> 21;
    const uint32_t immhi = (immed & 0x3ff000) >> 11;
    const uint32_t immlo = (immed & 0xffc) >> 2;
    const uint32_t J1 = ~I1 ^ S;
    const uint32_t J2 = ~I2 ^ S;
    return ((uint32_t)0x1E << 27) | (S << 26) | (immhi << 16) | ((uint32_t)0x3 << 14) | (J1 << 13) | (J2 << 11) | (immlo << 1);
}

uint32_t encode_thumb_inst(uint8_t type, uint32_t immed, uint16_t reg) {
    switch (type) {
    case INSTRUCTION_MOVW:
        return ((uint32_t)0x1E << 27) | ((uint32_t)(immed & 0x800) << 15) | ((uint32_t)0x24 << 20) | ((immed & 0xf000) << 4) | ((immed & 0x700) << 4) | (reg << 8) | (immed & 0xff);
    case INSTRUCTION_MOVT:
        return ((uint32_t)0x1E << 27) | ((uint32_t)(immed & 0x800) << 15) | ((uint32_t)0x2C << 20) | ((immed & 0xf000) << 4) | ((immed & 0x700) << 4) | (reg << 8) | (immed & 0xff);
    case INSTRUCTION_BRANCH:
        return ((((uint32_t)0x8E << 7) | (reg << 3)) << 16) | (uint32_t)0xBF00;
    case INSTRUCTION_BLX:
        return encode_thumb_blx(immed);
    case INSTRUCTION_UNKNOWN:
    default:
        return 0;
    }
}
