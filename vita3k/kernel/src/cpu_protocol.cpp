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
    if (svc == TRAMPOLINE_SVC) {
        Trampoline *tr = *Ptr<Trampoline *>(pc + 4).get(*mem);
        tr->callback(*kernel, cpu, *mem, tr->lr);
        return;
    }

    // TODO: just supply ThreadStatePtr to call_import
    // the only benefit of using thread_id instead--namely less locking--is now gone.
    ThreadStatePtr thread = lock_and_find(thread_id, kernel->threads, kernel->mutex);
    uint32_t nid = *Ptr<uint32_t>(pc + 4).get(*mem);
    call_import(cpu, nid, thread_id);
    if (!thread->jobs_to_add.empty()) {
        const auto current = thread->run_queue.begin();
        // Add resuming job
        ThreadJob job;
        job.ctx = save_context(cpu);
        job.notify = current->notify;
        thread->run_queue.erase(current);
        thread->run_queue.push_front(job);

        // Add requested callback jobs
        for (auto it = thread->jobs_to_add.rbegin(); it != thread->jobs_to_add.rend(); ++it) {
            thread->run_queue.push_front(*it);
        }
        thread->jobs_to_add.clear();
        stop(*thread->cpu);
    }
}

Address CPUProtocol::get_watch_memory_addr(Address addr) {
    return kernel->debugger.get_watch_memory_addr(addr);
}

std::vector<ModuleRegion> &CPUProtocol::get_module_regions() {
    return kernel->module_regions;
}

ExclusiveMonitorPtr CPUProtocol::get_exlusive_monitor() {
    return kernel->exclusive_monitor;
}
