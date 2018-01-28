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

#pragma once

#include <module/module.h>

// SceVideodecUser
BRIDGE_DECL(sceAvcdecCreateDecoder)
BRIDGE_DECL(sceAvcdecCsc)
BRIDGE_DECL(sceAvcdecCscInternal)
BRIDGE_DECL(sceAvcdecDecode)
BRIDGE_DECL(sceAvcdecDecodeAvailableSize)
BRIDGE_DECL(sceAvcdecDecodeFlush)
BRIDGE_DECL(sceAvcdecDecodeStop)
BRIDGE_DECL(sceAvcdecDeleteDecoder)
BRIDGE_DECL(sceAvcdecQueryDecoderMemSize)
BRIDGE_DECL(sceM4vdecCreateDecoder)
BRIDGE_DECL(sceM4vdecCsc)
BRIDGE_DECL(sceM4vdecDecode)
BRIDGE_DECL(sceM4vdecDecodeAvailableSize)
BRIDGE_DECL(sceM4vdecDecodeFlush)
BRIDGE_DECL(sceM4vdecDecodeStop)
BRIDGE_DECL(sceM4vdecDeleteDecoder)
BRIDGE_DECL(sceM4vdecQueryDecoderMemSize)
BRIDGE_DECL(sceVideodecInitLibrary)
BRIDGE_DECL(sceVideodecInitLibraryWithUnmapMem)
BRIDGE_DECL(sceVideodecQueryMemSize)
BRIDGE_DECL(sceVideodecTermLibrary)
