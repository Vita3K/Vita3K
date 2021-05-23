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

#include <kernel/cpu_protocol.h>
#include <kernel/state.h>
#include <util/lock_and_find.h>

CPUProtocol::CPUProtocol(KernelState &kernel, MemState &mem, const CallImportFunc &func)
    : call_import(func)
    , kernel(&kernel)
    , mem(&mem) {
}

CPUProtocol::~CPUProtocol() {
}

void CPUProtocol::call_svc(CPUState &cpu, uint32_t svc, Address pc, SceUID thread_id) {
    // Handle trampoline
    // 1. Handle trampoline jumper
    // to save the space we use interrupt to implement jumper
    // as thumb instructions require total three instructions to jump to any pc without limitations
    if (svc == TRAMPOLINE_JUMPER_SVC) {
        // find thumb16 trampoline
        Trampoline *tr = kernel->debugger.get_trampoline(pc - 2);
        // find thumb32 or arm trampoline
        if (!tr)
            tr = kernel->debugger.get_trampoline(pc - 4);
        write_pc(cpu, tr->trampoline_addr);
        return;
    }

    // 2. Call trampoline callback
    // this interrupt is made inside trampoline body
    if (svc == TRAMPOLINE_HANDLER_SVC) {
        Trampoline *tr = *Ptr<Trampoline *>(pc).get(*mem);
        tr->callback(cpu, *mem, tr->lr);
        return;
    }

    // This is usual service call
    uint32_t nid = *Ptr<uint32_t>(pc + 4).get(*mem);
    call_import(cpu, nid, thread_id);

    // TODO: just supply ThreadStatePtr to call_import
    // the only benefit of using thread_id instead--namely less locking--is now gone.
    ThreadStatePtr thread = lock_and_find(thread_id, kernel->threads, kernel->mutex);

    // Add callback jobs requested inside hle implementation
    thread->flush_callback_requests();

    // ARM recommends claering exclusive state inside interrupt handler
    clear_exclusive(kernel->exclusive_monitor, get_processor_id(cpu));
}

Address CPUProtocol::get_watch_memory_addr(Address addr) {
    return kernel->debugger.get_watch_memory_addr(addr);
}

ExclusiveMonitorPtr CPUProtocol::get_exlusive_monitor() {
    return kernel->exclusive_monitor;
}
