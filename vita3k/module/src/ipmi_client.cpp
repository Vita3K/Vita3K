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

#include <module/ipmi_client.h>

#include <kernel/state.h>
#include <mem/functions.h>
#include <util/log.h>

#include <array>

static Ptr<void> get_client_vtable_locked(ProcessIpmiObjects &objects, MemState &mem) {
    if (objects.client_vtable.get())
        return objects.client_vtable.get_ptr<void>();

    constexpr std::array<uint32_t, 16> nids{
        0x101C93F8, 0xA22C3E01, 0xEC73331C, 0xD484D36D,
        0x28BD5F19, 0x73C72FBB, 0xAFD10F3B, 0x387AFA3F,
        0xF8C2B8BA, 0x4EBB01A2, 0x8FF23C3C, 0x45C32034,
        0x004F48ED, 0xA3E650B0, 0x60EFADE7, 0xA5AA193C
    };
    constexpr uint32_t size = nids.size() * 4 * sizeof(uint32_t);
    const Address address = alloc(mem, size, "IPMI client vtable");
    if (!address) {
        LOG_ERROR("Failed to allocate IPMI client vtable");
        return {};
    }
    objects.client_vtable = Block(address, [&mem](Address addr) { free(mem, addr); });
    auto *function_pointer = Ptr<uint32_t>(address).get(mem);
    auto *function_svc = function_pointer + nids.size();
    Address function_location = address + nids.size() * sizeof(uint32_t);
    for (const uint32_t nid : nids) {
        *function_pointer++ = function_location;
        function_svc[0] = 0xef000000;
        function_svc[1] = 0xe1a0f00e;
        function_svc[2] = nid;
        function_svc += 3;
        function_location += 3 * sizeof(uint32_t);
    }
    return Ptr<void>(address);
}

Ptr<void> get_ipmi_client_vtable(KernelState &kernel, MemState &mem) {
    const std::lock_guard<std::mutex> lock(kernel.mutex);
    return get_client_vtable_locked(kernel.ipmi_objects_by_memory[&mem], mem);
}

Ptr<void> get_shellsvc_client(KernelState &kernel, MemState &mem) {
    const std::lock_guard<std::mutex> lock(kernel.mutex);
    auto &objects = kernel.ipmi_objects_by_memory[&mem];
    if (objects.shellsvc_client.get())
        return objects.shellsvc_client.get_ptr<void>();
    const Ptr<void> vtable = get_client_vtable_locked(objects, mem);
    if (!vtable)
        return {};
    const Address address = alloc(mem, sizeof(Address), "ShellSvc");
    if (!address) {
        LOG_ERROR("Failed to allocate ShellSvc client");
        return {};
    }
    objects.shellsvc_client = Block(address, [&mem](Address addr) { free(mem, addr); });
    *Ptr<Address>(address).get(mem) = vtable.address();
    return Ptr<void>(address);
}

SceInt32 initialize_ipmi_client(KernelState &kernel, MemState &mem, Ptr<void> *client, Ptr<void> client_memory) {
    if (!client || !client_memory) {
        LOG_ERROR("Cannot create IPMI client with null output or client memory");
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    }
    const Ptr<void> vtable = get_ipmi_client_vtable(kernel, mem);
    if (!vtable)
        return SCE_KERNEL_ERROR_NO_MEMORY;
    *client_memory.cast<Ptr<void>>().get(mem) = vtable;
    *client = client_memory;
    return SCE_KERNEL_OK;
}