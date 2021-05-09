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

#include "cpu_protocol.h"
#include <kernel/functions.h>
#include <modules/module_parent.h>

CPUProtocol::CPUProtocol(HostState &host)
    : host(&host) {
}

CPUProtocol::~CPUProtocol() {
}

void CPUProtocol::call_svc(CPUState &cpu, uint32_t svc, Address pc, SceUID thread_id) {
    uint32_t nid = *Ptr<uint32_t>(pc + 4).get(host->mem);
    ::call_import(*host, cpu, nid, thread_id);
}

Address CPUProtocol::get_watch_memory_addr(Address addr) {
    return ::get_watch_memory_addr(host->kernel, addr);
}

std::vector<ModuleRegion> &CPUProtocol::get_module_regions() {
    return host->kernel.module_regions;
}

ExclusiveMonitorPtr CPUProtocol::get_exlusive_monitor() {
    return host->kernel.exclusive_monitor;
}
