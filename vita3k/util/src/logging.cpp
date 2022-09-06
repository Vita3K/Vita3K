// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <util/log.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#else
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

#include <iostream>
#include <vector>

namespace logging {

static const fs::path &LOG_FILE_NAME = "vita3k.log";
static const char *LOG_PATTERN = "%^[%H:%M:%S.%e] |%L| [%!]: %v%$";
static std::vector<spdlog::sink_ptr> sinks;

static void register_log_exception_handler();

static void flush() {
    spdlog::details::registry::instance().flush_all();
}

ExitCode init(const Root &root_paths, bool use_stdout) {
    sinks.clear();
    if (use_stdout)
#ifdef __ANDROID__
        sinks.push_back(std::make_shared<spdlog::sinks::android_sink_mt>());
#else
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
#endif

    if (add_sink(root_paths.get_log_path() / LOG_FILE_NAME) != Success)
        return InitConfigFailed;

    spdlog::set_error_handler([](const std::string &msg) {
        std::cerr << "spdlog error: " << msg << std::endl;
        assert(0);
    });

#ifdef _WIN32
    // set console codepage to UTF-8
    SetConsoleOutputCP(65001);
    SetConsoleTitle("Vita3K PSVita Emulator");
#endif

#ifdef __ANDROID__
    // needed, otherwise the log file contains nothing
    spdlog::flush_on(spdlog::level::trace);
#endif

    register_log_exception_handler();

    static std::terminate_handler old_terminate = nullptr;
    old_terminate = std::set_terminate([]() {
        try {
            std::rethrow_exception(std::current_exception());
        } catch (const std::exception &e) {
            LOG_CRITICAL("Unhandled C++ exception. {}", e.what());
        } catch (...) {
            LOG_CRITICAL("Unhandled C++ exception. UNKNOWN");
        }
        flush();
        if (old_terminate)
            old_terminate();
    });
    return Success;
}

void set_level(spdlog::level::level_enum log_level) {
    spdlog::set_level(log_level);
}

ExitCode add_sink(const fs::path &log_path) {
    try {
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.generic_path().native(), true));
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

// log exceptions and flush log file on exceptions
#ifdef _WIN32
static LONG WINAPI exception_handler(PEXCEPTION_POINTERS pExp) noexcept {
    const unsigned ec = pExp->ExceptionRecord->ExceptionCode;
    switch (ec) {
    case EXCEPTION_ACCESS_VIOLATION:
        LOG_CRITICAL("Exception EXCEPTION_ACCESS_VIOLATION ({}). ", log_hex(ec));
        switch (pExp->ExceptionRecord->ExceptionInformation[0]) {
        case 0:
            LOG_CRITICAL("Read violation at address {}.", log_hex(pExp->ExceptionRecord->ExceptionInformation[1]));
            break;
        case 1:
            LOG_CRITICAL("Write violation at address {}.", log_hex(pExp->ExceptionRecord->ExceptionInformation[1]));
            break;
        case 8:
            LOG_CRITICAL("DEP violation at address {}.", log_hex(pExp->ExceptionRecord->ExceptionInformation[1]));
            break;
        default:
            break;
        }
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        LOG_CRITICAL("Exception EXCEPTION_ARRAY_BOUNDS_EXCEEDED ({}). ", log_hex(ec));
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        LOG_CRITICAL("Exception EXCEPTION_DATATYPE_MISALIGNMENT ({}). ", log_hex(ec));
        break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        LOG_CRITICAL("Exception EXCEPTION_FLT_DIVIDE_BY_ZERO ({}). ", log_hex(ec));
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        LOG_CRITICAL("Exception EXCEPTION_ILLEGAL_INSTRUCTION ({}). ", log_hex(ec));
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        LOG_CRITICAL("Exception EXCEPTION_IN_PAGE_ERROR ({}). ", log_hex(ec));
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        LOG_CRITICAL("Exception EXCEPTION_INT_DIVIDE_BY_ZERO ({}). ", log_hex(ec));
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        LOG_CRITICAL("Exception EXCEPTION_PRIV_INSTRUCTION ({}). ", log_hex(ec));
        break;
    case EXCEPTION_STACK_OVERFLOW:
        LOG_CRITICAL("Exception EXCEPTION_STACK_OVERFLOW ({}). ", log_hex(ec));
        break;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }
    flush();
    return EXCEPTION_CONTINUE_SEARCH;
}

void register_log_exception_handler() {
    if (!AddVectoredExceptionHandler(0, exception_handler)) {
        LOG_CRITICAL("Failed to register an exception handler");
    }
}

#else
void register_log_exception_handler() {}
#endif
} // namespace logging
