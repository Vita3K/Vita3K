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

#include <host/functions.h>

#include <cpu/functions.h>
#include <host/import_fn.h>
#include <host/state.h>
#include <nids/functions.h>
#include <util/lock_and_find.h>
#include <util/log.h>

static bool LOG_IMPORT_CALLS = false;
static constexpr bool LOG_UNK_NIDS_ALWAYS = false;

#define NID(name, nid) extern const ImportFn import_##name;
#include <nids/nids.h>
#undef NID

static ImportFn resolve_import(uint32_t nid) {
    switch (nid) {
#define NID(name, nid) \
    case nid:          \
        return import_##name;
#include <nids/nids.h>
#undef NID
    }

    return ImportFn();
}

/**
 * \brief Resolves a function imported from a loaded module.
 * \param kernel Kernel state struct
 * \param nid NID to resolve
 * \return Resolved address, 0 if not found
 */
static Address resolve_export(KernelState &kernel, uint32_t nid) {
    const ExportNids::iterator export_address = kernel.export_nids.find(nid);
    if (export_address == kernel.export_nids.end()) {
        return 0;
    }

    return export_address->second;
}

void call_import(HostState &host, CPUState &cpu, uint32_t nid, SceUID thread_id) {
    Address export_pc = resolve_export(host.kernel, nid);

    if (!export_pc) {
        // HLE - call our C++ function

        if (LOG_IMPORT_CALLS) {
            const char *const name = import_name(nid);
            LOG_TRACE("THREAD_ID {} NID {} ({}) called", thread_id, log_hex(nid), name);
        }
        const ImportFn fn = resolve_import(nid);
        if (fn) {
            fn(host, cpu, thread_id);
        } else if (host.missing_nids.count(nid) == 0 || LOG_UNK_NIDS_ALWAYS) {
            const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
            LOG_ERROR("Import function for NID {} not found (thread name: {}, thread ID: {})", log_hex(nid), thread->name, thread_id);

            if (!LOG_UNK_NIDS_ALWAYS)
                host.missing_nids.insert(nid);
        }
    } else {
        // LLE - directly run ARM code imported from some loaded module

        if (LOG_IMPORT_CALLS) {
            const char *const name = import_name(nid);
            LOG_TRACE("THREAD_ID {} EXPORTED NID {} at {} ({})) called", thread_id, log_hex(nid), log_hex(export_pc), name);
        }
        const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
        const std::lock_guard<std::mutex> lock(thread->mutex);
        write_pc(*thread->cpu, export_pc);
    }
}

void log_import_calls(bool enabled) {
    LOG_IMPORT_CALLS = enabled;
}
