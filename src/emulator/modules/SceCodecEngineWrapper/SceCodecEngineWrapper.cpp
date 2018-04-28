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

#include "SceCodecEngineWrapper.h"

EXPORT(int, _sceCodecEngineAllocMemoryFromUnmapMemBlock) {
    return unimplemented(export_name);
}

EXPORT(int, _sceCodecEngineCloseUnmapMemBlock) {
    return unimplemented(export_name);
}

EXPORT(int, _sceCodecEngineFreeMemoryFromUnmapMemBlock) {
    return unimplemented(export_name);
}

EXPORT(int, _sceCodecEngineOpenUnmapMemBlock) {
    return unimplemented(export_name);
}

BRIDGE_IMPL(_sceCodecEngineAllocMemoryFromUnmapMemBlock)
BRIDGE_IMPL(_sceCodecEngineCloseUnmapMemBlock)
BRIDGE_IMPL(_sceCodecEngineFreeMemoryFromUnmapMemBlock)
BRIDGE_IMPL(_sceCodecEngineOpenUnmapMemBlock)
