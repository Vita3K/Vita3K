#pragma once

#include <spdlog/spdlog.h>

extern std::shared_ptr<spdlog::logger> g_logger;

void init_logging();

#define LOG_TRACE(fmt, ...) g_logger->trace("[ {:s} ]:    " fmt, __FUNCTION__, ## __VA_ARGS__)
#define LOG_DEBUG(fmt, ...) g_logger->debug("[ {:s} ]:    " fmt, __FUNCTION__, ## __VA_ARGS__)
#define LOG_INFO(fmt, ...) g_logger->info("[ {:s} ]:    " fmt, __FUNCTION__, ## __VA_ARGS__)
#define LOG_WARN(fmt, ...) g_logger->warn("[ {:s} ]:    " fmt, __FUNCTION__, ## __VA_ARGS__)
#define LOG_ERROR(fmt, ...) g_logger->error("[ {:s} ]:    " fmt, __FUNCTION__, ## __VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) g_logger->critical("[ {:s} ]:    " fmt, __FUNCTION__, ## __VA_ARGS__)

#define LOG_TRACE_IF(flag, fmt, ...) g_logger->trace_if(flag, "[ {:s} ]:    " fmt, __FUNCTION__, ## __VA_ARGS__)
#define LOG_DEBUG_IF(flag, fmt, ...) g_logger->debug_if(flag, "[ {:s} ]:    " fmt, __FUNCTION__, ## __VA_ARGS__)
#define LOG_INFO_IF(flag, fmt, ...) g_logger->info_if(flag, "[ {:s} ]:    " fmt, __FUNCTION__, ## __VA_ARGS__)
#define LOG_WARN_IF(flag, fmt, ...) g_logger->warn_if(flag, "[ {:s} ]:    " fmt, __FUNCTION__, ## __VA_ARGS__)
#define LOG_ERROR_IF(flag, fmt, ...) g_logger->error_if(flag, "[ {:s} ]:    " fmt, __FUNCTION__, ## __VA_ARGS__)
#define LOG_CRITICAL_IF(flag, fmt, ...) g_logger->critical_if(flag, "[ {:s} ]:    " fmt, __FUNCTION__, ## __VA_ARGS__)
