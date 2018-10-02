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

#include <type_traits>

namespace logging {

extern std::shared_ptr<spdlog::logger> g_logger;

void init();
void set_level(spdlog::level::level_enum log_level);

#define LOG_TRACE(fmt, ...) logging::g_logger->trace("[{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) logging::g_logger->debug("[{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) logging::g_logger->info("[{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) logging::g_logger->warn("[{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) logging::g_logger->error("[{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) logging::g_logger->critical("[{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)

#define LOG_TRACE_IF(flag, fmt, ...) \
    if (flag)                        \
    logging::g_logger->trace("[{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_DEBUG_IF(flag, fmt, ...) \
    if (flag)                        \
    logging::g_logger->debug("[{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_INFO_IF(flag, fmt, ...) \
    if (flag)                       \
    logging::g_logger->info("[{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_WARN_IF(flag, fmt, ...) \
    if (flag)                       \
    logging::g_logger->warn("[{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_ERROR_IF(flag, fmt, ...) \
    if (flag)                        \
    logging::g_logger->error("[{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_CRITICAL_IF(flag, fmt, ...) \
    if (flag)                           \
    logging::g_logger->critical("[{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)

} // namespace logging

template <typename T>
std::string log_hex(T val) {
    using unsigned_type = typename std::make_unsigned<T>::type;
    return fmt::format("0x{:0X}", static_cast<unsigned_type>(val));
}
