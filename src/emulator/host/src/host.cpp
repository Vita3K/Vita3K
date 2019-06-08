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

#include <unordered_set>

#ifdef NDEBUG // Leave it as non-constexpr on Debug so that we can enable/disable it at will via set_log_import_calls
constexpr
#endif
    bool LOG_IMPORT_CALLS
    = false;

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

static void log_import_call(char emulation_level, uint32_t nid, SceUID thread_id, const std::unordered_set<uint32_t> &nid_blacklist) {
    if (nid_blacklist.find(nid) == nid_blacklist.end()) {
        const char *const name = import_name(nid);
        LOG_TRACE("[{}LE] TID: {:<3} FUNC: {} {}", emulation_level, thread_id, log_hex(nid), name);
    }
}

void call_import(HostState &host, CPUState &cpu, uint32_t nid, SceUID thread_id) {
    Address export_pc = resolve_export(host.kernel, nid);

    if (!export_pc) {
        // HLE - call our C++ function

        if (LOG_IMPORT_CALLS) {
            const std::unordered_set<uint32_t> hle_nid_blacklist = {
                0xB295EB61, // sceKernelGetTLSAddr
                0x46E7BE7B, // sceKernelLockLwMutex
                0x91FA6614, // sceKernelUnlockLwMutex
            };

            log_import_call('H', nid, thread_id, hle_nid_blacklist);
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
            const std::unordered_set<uint32_t> lle_nid_blacklist = {};

            log_import_call('L', nid, thread_id, lle_nid_blacklist);
        }
        const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
        const std::lock_guard<std::mutex> lock(thread->mutex);
        write_pc(*thread->cpu, export_pc);
    }
}

#ifndef NDEBUG
// Import logging is really slow and this allows us to enable/disable it from whenever we please, for easy in debugging
void set_log_import_calls(bool enabled) {
    LOG_IMPORT_CALLS = enabled;
}
#endif
