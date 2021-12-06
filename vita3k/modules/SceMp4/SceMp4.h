// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

BRIDGE_DECL(sceMp4CloseFile)
BRIDGE_DECL(sceMp4EnableStream)
BRIDGE_DECL(sceMp4GetNextUnit)
BRIDGE_DECL(sceMp4GetNextUnit2Ref)
BRIDGE_DECL(sceMp4GetNextUnit3Ref)
BRIDGE_DECL(sceMp4GetNextUnitData)
BRIDGE_DECL(sceMp4GetStreamInfo)
BRIDGE_DECL(sceMp4JumpPTS)
BRIDGE_DECL(sceMp4OpenFile)
BRIDGE_DECL(sceMp4PTSToTime)
BRIDGE_DECL(sceMp4ReleaseBuffer)
BRIDGE_DECL(sceMp4Reset)
BRIDGE_DECL(sceMp4StartFileStreaming)
BRIDGE_DECL(sceMp4TimeToPTS)
BRIDGE_DECL(_sceMp4GetStreamInfo)
