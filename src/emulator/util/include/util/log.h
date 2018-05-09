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

extern std::shared_ptr<spdlog::logger> g_logger;

void init_logging();

#define LOG_TRACE(fmt, ...) g_logger->trace("|T| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) g_logger->debug("|D| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) g_logger->info("|I| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) g_logger->warn("|W| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) g_logger->error("|E| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) g_logger->critical("|C| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)

#define LOG_TRACE_IF(flag, fmt, ...) \
    if (flag)                        \
    g_logger->trace(flag, "|T| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_DEBUG_IF(flag, fmt, ...) \
    if (flag)                        \
    g_logger->debug(flag, "|D| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_INFO_IF(flag, fmt, ...) \
    if (flag)                       \
    g_logger->info(flag, "|I| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_WARN_IF(flag, fmt, ...) \
    if (flag)                       \
    g_logger->warn(flag, "|W| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_ERROR_IF(flag, fmt, ...) \
    if (flag)                        \
    g_logger->error(flag, "|E| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_CRITICAL_IF(flag, fmt, ...) \
    if (flag)                           \
    g_logger->critical(flag, "|C| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
