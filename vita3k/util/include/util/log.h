// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#pragma once

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <spdlog/spdlog.h>
#include <util/exit_code.h>
#include <util/fs.h>

#include <type_traits>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#define LOG_TRACE SPDLOG_TRACE
#define LOG_DEBUG SPDLOG_DEBUG
#define LOG_INFO SPDLOG_INFO
#define LOG_WARN SPDLOG_WARN
#define LOG_ERROR SPDLOG_ERROR
#define LOG_CRITICAL SPDLOG_CRITICAL

#define LOG_TRACE_IF(flag, ...) \
    if (flag)                   \
    LOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG_IF(flag, ...) \
    if (flag)                   \
    LOG_DEBUG(__VA_ARGS__)
#define LOG_INFO_IF(flag, ...) \
    if (flag)                  \
    LOG_INFO(__VA_ARGS__)
#define LOG_WARN_IF(flag, ...) \
    if (flag)                  \
    LOG_WARN(__VA_ARGS__)
#define LOG_ERROR_IF(flag, ...) \
    if (flag)                   \
    LOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL_IF(flag, ...) \
    if (flag)                      \
    LOG_CRITICAL(__VA_ARGS__)

#define LOG_ONCE(log_function, ...)    \
    do {                               \
        static bool LOG_DONE = false;  \
        if (!LOG_DONE)                 \
            log_function(__VA_ARGS__); \
        LOG_DONE = true;               \
    } while (0)

#define LOG_TRACE_ONCE(...) LOG_ONCE(LOG_TRACE, __VA_ARGS__)
#define LOG_DEBUG_ONCE(...) LOG_ONCE(LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO_ONCE(...) LOG_ONCE(LOG_INFO, __VA_ARGS__)
#define LOG_WARN_ONCE(...) LOG_ONCE(LOG_WARN, __VA_ARGS__)
#define LOG_ERROR_ONCE(...) LOG_ONCE(LOG_ERROR, __VA_ARGS__)
#define LOG_CRITICAL_ONCE(...) LOG_ONCE(LOG_CRITICAL, __VA_ARGS__)

namespace logging {

ExitCode init(const Root &root_paths, bool use_stdout);
void set_level(spdlog::level::level_enum log_level);
ExitCode add_sink(const fs::path &log_path);

} // namespace logging

#define RET_ERROR(error)                                                            \
    ([&]() {                                                                        \
        LOG_ERROR_ONCE("{} returned {} ({})", export_name, #error, log_hex(error)); \
        return static_cast<int>(error);                                             \
    })()

// Using stringstream as its 2x faster than fmt::format

/*
    returns: A string with the input number formatted in hexadecimal
    Examples:
        * `12` returns: `"0xC"`
        * `1337` returns: `"0x539"`
        * `72742069` returns: `"0x455F4B5"`
*/
template <typename T>
std::string log_hex(T val) {
    using unsigned_type = typename std::make_unsigned<T>::type;
    std::stringstream ss;
    ss << "0x";
    ss << std::hex << static_cast<unsigned_type>(val);
    return ss.str();
}

/*
    returns: A string with the input number formatted in hexadecimal with padding of the inputted type size
    Examples:
        * `uint8_t 5` returns: `"0x05"`
        * `uint8_t 15` returns: `"0x0F"`
        * `uint8_t 255` returns: `"0xFF"`

        * `uint16_t 15` returns: `"0x000F"`
        * `uint16_t 1337` returns: `"0x0539"`
        * `uint16_t 65535` returns: `"0xFFFF"`


        * `uint32_t 15` returns: `"0x0000000F"`
        * `uint32_t 1337` returns: `"0x00000539"`
        * `uint32_t 65535` returns: `"0x0000FFFF"`
        * `uint32_t 134217728` returns: `"0x08000000"`
*/
template <typename T>
std::string log_hex_full(T val) {
    std::stringstream ss;
    ss << "0x";
    ss << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex << val;
    return ss.str();
}

template <class T>
class Ptr;
FMT_BEGIN_NAMESPACE
template <typename T, typename Char>
struct formatter<Ptr<T>, Char> : formatter<string_view, Char> {
public:
    template <typename FormatContext>
    auto format(const Ptr<T> p, FormatContext &ctx) const {
        return detail::write(ctx.out(),
            basic_string_view<Char>(log_hex_full(p.address())));
    }
};
FMT_END_NAMESPACE
