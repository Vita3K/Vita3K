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

#include "SceCodecEnginePerf.h"

EXPORT(int, sceCodecEnginePmonGetProcessorLoad) {
    return unimplemented(export_name);
}

EXPORT(int, sceCodecEnginePmonReset) {
    return unimplemented(export_name);
}

EXPORT(int, sceCodecEnginePmonStart) {
    return unimplemented(export_name);
}

EXPORT(int, sceCodecEnginePmonStop) {
    return unimplemented(export_name);
}

BRIDGE_IMPL(sceCodecEnginePmonGetProcessorLoad)
BRIDGE_IMPL(sceCodecEnginePmonReset)
BRIDGE_IMPL(sceCodecEnginePmonStart)
BRIDGE_IMPL(sceCodecEnginePmonStop)
