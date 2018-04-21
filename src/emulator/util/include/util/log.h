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

#define LOG_TRACE_IF(flag, fmt, ...) g_logger->trace_if(flag, "|T| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_DEBUG_IF(flag, fmt, ...) g_logger->debug_if(flag, "|D| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_INFO_IF(flag, fmt, ...) g_logger->info_if(flag, "|I| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_WARN_IF(flag, fmt, ...) g_logger->warn_if(flag, "|W| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_ERROR_IF(flag, fmt, ...) g_logger->error_if(flag, "|E| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
#define LOG_CRITICAL_IF(flag, fmt, ...) g_logger->critical_if(flag, "|C| [{:s}]:  " fmt, __FUNCTION__, ##__VA_ARGS__)
