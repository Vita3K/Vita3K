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

#include <module/module.h>

#include <kernel/state.h>
#include <util/lock_and_find.h>
#include <v3kprintf.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceDbg);

EXPORT(int, sceDbgAssertionHandler, const char *filename, int line, bool do_stop, const char *component, module::vargs messages) {
    TRACY_FUNC(sceDbgAssertionHandler, filename, line, do_stop, component);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);

    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    std::vector<char> buffer(KiB(1));

    const char *main_message = messages.next<Ptr<const char>>(*(thread->cpu), emuenv.mem).get(emuenv.mem);
    const int result = utils::snprintf(buffer.data(), buffer.size(), main_message, *(thread->cpu), emuenv.mem, messages);

    LOG_INFO("file {}, line {}, {}", filename, line, buffer.data());

    if (do_stop)
        emuenv.kernel.exit_delete_all_threads();

    if (!result) {
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }

    assert(!do_stop);

    return 0;
}

EXPORT(int, sceDbgLoggingHandler, const char *pFile, int line, int severity, const char *pComponent, module::vargs messages) {
    TRACY_FUNC(sceDbgLoggingHandler, pFile, line, severity, pComponent);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);

    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    std::string output = fmt::format("SCE libdbg LOG, LEVEL: {}", severity);

    if (pComponent && (pComponent[0] != '\0')) {
        output += fmt::format(", COMPONENT: {}", pComponent);
    }

    if (pFile && (pFile[0] != '\0')) {
        output += fmt::format(", FILE:{}, LINE:{}", pFile, line);
    }

    std::vector<char> buffer(KiB(1));

    const char *main_message = messages.next<Ptr<const char>>(*(thread->cpu), emuenv.mem).get(emuenv.mem);
    const int result = utils::snprintf(buffer.data(), buffer.size(), main_message, *(thread->cpu), emuenv.mem, messages);

    if (result) {
        output += fmt::format(" {}", buffer.data());
    }

    LOG_INFO(output);

    return 0;
}

EXPORT(int, sceDbgSetBreakOnErrorState) {
    TRACY_FUNC(sceDbgSetBreakOnErrorState);
    return UNIMPLEMENTED();
}

EXPORT(int, sceDbgSetBreakOnWarningState) {
    TRACY_FUNC(sceDbgSetBreakOnWarningState);
    return UNIMPLEMENTED();
}

EXPORT(int, sceDbgSetMinimumLogLevel) {
    TRACY_FUNC(sceDbgSetMinimumLogLevel);
    return UNIMPLEMENTED();
}
