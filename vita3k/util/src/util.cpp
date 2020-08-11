// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <util/bytes.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <codecvt> // std::codecvt_utf8
#include <iostream>
#include <locale> // std::wstring_convert
#include <memory>
#include <set>
#include <sstream>
#include <string>

#include <memory>
#include <stdexcept>

#include <immintrin.h>

namespace logging {

static const fs::path &LOG_FILE_NAME = "vita3k.log";
static const char *LOG_PATTERN = "%^[%H:%M:%S.%e] |%L| [%!]: %v%$";
std::vector<spdlog::sink_ptr> sinks;

ExitCode init(const Root &root_paths, bool use_stdout) {
    sinks.clear();
    if (use_stdout)
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    if (add_sink(root_paths.get_base_path_string() / LOG_FILE_NAME) != Success)
        return InitConfigFailed;

    spdlog::set_error_handler([](const std::string &msg) {
        std::cerr << "spdlog error: " << msg << std::endl;
        assert(0);
    });

    spdlog::flush_on(spdlog::level::debug);
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

void float_to_half(const float *src, std::uint16_t *dest, const int total) {
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

// based on https://gist.github.com/shreyasbharath/32a8092666303a916e24a81b18af146b
void dump_hex(const std::vector<uint8_t> &bytes, std::ostream &stream) {
    char buff[17];
    size_t i = 0;

    stream << std::hex;

    // Process every byte in the data.
    for (i = 0; i < bytes.size(); i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0) {
                stream << "  " << buff << std::endl;
            }

            // Output the offset.
            stream << "  " << std::setw(4) << std::setfill('0') << static_cast<unsigned int>(i);
        }

        // Now the hex code for the specific character.
        stream << " " << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(bytes[i]);

        // And store a printable ASCII character for later.
        if ((bytes[i] < 0x20) || (bytes[i] > 0x7e)) {
            buff[i % 16] = '.';
        } else {
            buff[i % 16] = bytes[i];
        }
        buff[(i % 16) + 1] = '\0';
    }

    stream << std::dec;

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        stream << "   ";
        i++;
    }

    // And print the final ASCII bit.
    stream << "  " << buff << std::endl;
}
