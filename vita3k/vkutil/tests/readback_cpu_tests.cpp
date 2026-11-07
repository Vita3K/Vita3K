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

#define VMA_IMPLEMENTATION
#include <cstring>
#include <iostream>
#include <mem/functions.h>
#include <mem/state.h>
#include <stdexcept>
#include <vkutil/readback.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
static void require(bool pass, const char *message) {
    if (!pass)
        throw std::runtime_error(message);
}
#if defined(__APPLE__)
static vm_prot_t protection(uint8_t *p) {
    mach_vm_address_t address = reinterpret_cast<mach_vm_address_t>(p);
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t region{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = 0;
    require(mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64,
                reinterpret_cast<vm_region_info_t>(&region), &count, &object)
            == KERN_SUCCESS,
        "Host permission query");
    if (object)
        mach_port_deallocate(mach_task_self(), object);
    return region.protection;
}
#endif
int main() try {
    using S = vk::ComponentSwizzle;
    std::vector<uint8_t> input = { 0x00, 0xf8, 0xe0, 0x07, 0x1f, 0x00, 0xff, 0xff }, out;
    require(vkutil::convert_readback_pixels(vk::Format::eR5G6B5UnormPack16, { S::eR, S::eG, S::eB, S::eOne }, 2, input, out) && out == input, "RGB565 native packing");
    require(vkutil::convert_readback_pixels(vk::Format::eR5G6B5UnormPack16, { S::eB, S::eG, S::eR, S::eOne }, 2, input, out) && out == std::vector<uint8_t>({ 0x1f, 0, 0xe0, 7, 0, 0xf8, 0xff, 0xff }), "RGB565 R/B exchange");
    input = { 11, 22, 33, 44 };
    const vk::ComponentMapping maps[] = { { S::eR, S::eG, S::eB, S::eA }, { S::eB, S::eG, S::eR, S::eA }, { S::eA, S::eB, S::eG, S::eR }, { S::eG, S::eB, S::eA, S::eR } };
    const std::vector<uint8_t> expected[] = { { 11, 22, 33, 44 }, { 33, 22, 11, 44 }, { 44, 33, 22, 11 }, { 44, 11, 22, 33 } };
    for (size_t i = 0; i < 4; ++i)
        require(vkutil::convert_readback_pixels(vk::Format::eR8G8B8A8Unorm, maps[i], 4, input, out) && out == expected[i], "RGBA swizzle");
    require(vkutil::convert_readback_pixels(vk::Format::eR8G8B8A8Unorm, { S::eR, S::eG, S::eB, S::eOne }, 3, input, out) && out == std::vector<uint8_t>({ 11, 22, 33 }), "FVS RGB24 tight packing");
    out = { 99 };
    require(!vkutil::convert_readback_pixels(vk::Format::eR8G8B8A8Unorm, { S::eR, S::eB, S::eG, S::eA }, 4, input, out) && out == std::vector<uint8_t>({ 99 }), "Unsupported mapping changed output");
    require(!vkutil::convert_readback_pixels(vk::Format::eUndefined, maps[0], 4, input, out) && out == std::vector<uint8_t>({ 99 }), "Unsupported format changed output");
    MemState mem;
    require(init(mem, false), "Guest memory init");
    const uint32_t size = 128 * 1024;
    const Address base = alloc(mem, size, "readback CPU fixture");
    require(base, "Guest memory alloc");
    std::memset(mem.memory.get() + base, 0xa5, size);
    const uint32_t row = 192, stride = 208, height = 200;
    std::vector<uint8_t> pixels(row * height);
    for (size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = uint8_t(i * 37 + 11);
    unsigned callbacks = 0;
    require(add_protect(mem, base, 65536, MemPerm::None, [&](Address, bool) {++callbacks;return true; }), "Protection setup");
    require(write_gpu_readback_rows(mem, base, pixels.data(), row, height, stride), "Protected rows commit");
    require(callbacks == 0 && mem.protect_tree.size() == 1, "GPU commit fired CPU invalidation");
#if defined(__APPLE__)
    require(protection(mem.memory.get() + base) == VM_PROT_NONE, "Protection not restored");
#endif
    require(handle_access_violation(mem, mem.memory.get() + base, false), "Protection callback lost");
    require(callbacks == 1, "Protection callback count");
    for (unsigned y = 0; y < height; ++y) {
        require(std::memcmp(mem.memory.get() + base + y * stride, pixels.data() + y * row, row) == 0, "Row bytes");
        for (unsigned i = row; i < stride; ++i)
            require(mem.memory.get()[base + y * stride + i] == 0xa5, "Row padding");
    }
    require(mem.memory.get()[base + height * stride] == 0xa5, "Adjacent sentinel");
    std::vector<uint8_t> before(mem.memory.get() + base, mem.memory.get() + base + size);
    require(!write_gpu_readback_rows(mem, 0, pixels.data(), row, height, stride), "Null accepted");
    require(!write_gpu_readback_rows(mem, base, pixels.data(), row, height, row - 1), "Short stride accepted");
    require(!write_gpu_readback_rows(mem, base + size - 8, pixels.data(), 16, 1, 16), "Unallocated end accepted");
    require(!write_gpu_readback_rows(mem, 0xfffffff0, pixels.data(), 32, 1, 32), "Overflow accepted");
    require(std::memcmp(mem.memory.get() + base, before.data(), size) == 0, "Failed commit partially wrote");
#if defined(__APPLE__)
    require(mem.host_page_size == 16384, "Expected macOS16KiB host pages");
    const uint8_t data[16] = { 1, 2, 3, 4 };
    callbacks = 0;
    require(add_protect(mem, base, 4096, MemPerm::ReadOnly, [&](Address, bool) {++callbacks;return true; }), "Mixed RO");
    require(add_protect(mem, base + 8192, 4096, MemPerm::None, [&](Address, bool) {++callbacks;return true; }), "Mixed None");
    require(write_gpu_readback_rows(mem, base + 4096, data, 16, 1, 16), "Mixed commit");
    require(protection(mem.memory.get() + base) == VM_PROT_NONE && callbacks == 0 && mem.protect_tree.size() == 2, "Shared page permission/metadata");
    require(handle_access_violation(mem, mem.memory.get() + base + 8192, false) && handle_access_violation(mem, mem.memory.get() + base, true) && callbacks == 2, "Mixed callbacks");
    callbacks = 0;
    require(add_protect(mem, base, 5 * 4096, MemPerm::ReadOnly, [&](Address, bool) {++callbacks;return true; }), "Long RO");
    require(add_protect(mem, base + 6 * 4096, 4096, MemPerm::None, [&](Address, bool) {++callbacks;return true; }), "Neighbour None");
    require(write_gpu_readback_rows(mem, base + 4096, data, 16, 1, 16), "Clipped commit");
    require(protection(mem.memory.get() + base + 6 * 4096) == VM_PROT_NONE && callbacks == 0 && mem.protect_tree.size() == 2, "Neighbour page weakened");
    require(handle_access_violation(mem, mem.memory.get() + base + 6 * 4096, false) && handle_access_violation(mem, mem.memory.get() + base, true) && callbacks == 2, "Neighbour callbacks");
#endif
    free(mem, base);
    std::cout << "{\"passed\":true,\"pixel_conversion_cases\":9,\"protected_rows\":200,\"invalid_destination_cases\":4,\"mixed_permission_cases\":2,\"gpu_used\":false}" << std::endl;
    return 0;
} catch (const std::exception &error) {
    std::cerr << error.what() << std::endl;
    return 1;
}
