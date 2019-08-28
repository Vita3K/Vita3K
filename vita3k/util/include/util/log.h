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

#pragma once

#include <spdlog/spdlog.h>
#include <util/exit_code.h>
#include <util/fs.h>

#include <type_traits>

namespace logging {

ExitCode init(const Root &root_paths);
void set_level(spdlog::level::level_enum log_level);
ExitCode add_sink(const fs::path &log_path);

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

int ret_error_impl(const char *name, const char *error_str, std::uint32_t error_val);
} // namespace logging

#define RET_ERROR(error) logging::ret_error_impl(export_name, #error, error)

template <typename T>
std::string log_hex(T val) {
    using unsigned_type = typename std::make_unsigned<T>::type;
    return fmt::format("0x{:0X}", static_cast<unsigned_type>(val));
}
