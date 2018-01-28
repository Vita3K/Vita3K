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

#include <SceHeap/exports.h>

EXPORT(int, sceHeapAllocHeapMemory) {
    return unimplemented("sceHeapAllocHeapMemory");
}

EXPORT(int, sceHeapAllocHeapMemoryWithOption) {
    return unimplemented("sceHeapAllocHeapMemoryWithOption");
}

EXPORT(int, sceHeapCreateHeap) {
    return unimplemented("sceHeapCreateHeap");
}

EXPORT(int, sceHeapDeleteHeap) {
    return unimplemented("sceHeapDeleteHeap");
}

EXPORT(int, sceHeapFreeHeapMemory) {
    return unimplemented("sceHeapFreeHeapMemory");
}

EXPORT(int, sceHeapGetMallinfo) {
    return unimplemented("sceHeapGetMallinfo");
}

EXPORT(int, sceHeapGetTotalFreeSize) {
    return unimplemented("sceHeapGetTotalFreeSize");
}

EXPORT(int, sceHeapIsAllocatedHeapMemory) {
    return unimplemented("sceHeapIsAllocatedHeapMemory");
}

EXPORT(int, sceHeapReallocHeapMemory) {
    return unimplemented("sceHeapReallocHeapMemory");
}

EXPORT(int, sceHeapReallocHeapMemoryWithOption) {
    return unimplemented("sceHeapReallocHeapMemoryWithOption");
}

BRIDGE_IMPL(sceHeapAllocHeapMemory)
BRIDGE_IMPL(sceHeapAllocHeapMemoryWithOption)
BRIDGE_IMPL(sceHeapCreateHeap)
BRIDGE_IMPL(sceHeapDeleteHeap)
BRIDGE_IMPL(sceHeapFreeHeapMemory)
BRIDGE_IMPL(sceHeapGetMallinfo)
BRIDGE_IMPL(sceHeapGetTotalFreeSize)
BRIDGE_IMPL(sceHeapIsAllocatedHeapMemory)
BRIDGE_IMPL(sceHeapReallocHeapMemory)
BRIDGE_IMPL(sceHeapReallocHeapMemoryWithOption)
