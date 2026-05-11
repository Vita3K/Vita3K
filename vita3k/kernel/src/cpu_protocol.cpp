// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <kernel/cpu_protocol.h>

#include <cpu/functions.h>
#include <kernel/state.h>

CPUProtocol::CPUProtocol(KernelState &kernel, MemState &mem, const CallImportFunc &func)
    : call_import(func)
    , kernel(&kernel)
    , mem(&mem) {
}

void CPUProtocol::call_svc(CPUState &cpu, uint32_t svc, Address pc, ThreadState &thread) {
    uint32_t nid = *Ptr<uint32_t>(pc + 4).get(*mem);
    // TODO: just supply ThreadStatePtr to call_import
    // the only benefit of using thread_id instead--namely less locking-- has been gone for long
    call_import(cpu, nid, thread.id);

    // ARM recommends clearing exclusive state inside interrupt handler
    clear_exclusive(cpu);
}

Address CPUProtocol::get_watch_memory_addr(Address addr) {
    return kernel->debugger.get_watch_memory_addr(addr);
}
