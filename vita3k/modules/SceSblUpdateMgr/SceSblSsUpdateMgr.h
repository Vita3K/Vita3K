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

BRIDGE_DECL(sceSblUsAllocateBuffer)
BRIDGE_DECL(sceSblUsCheckSystemIntegrity)
BRIDGE_DECL(sceSblUsExtractSpackage)
BRIDGE_DECL(sceSblUsGetApplicableVersion)
BRIDGE_DECL(sceSblUsGetExtractSpackage)
BRIDGE_DECL(sceSblUsGetSpkgInfo)
BRIDGE_DECL(sceSblUsGetStatus)
BRIDGE_DECL(sceSblUsGetUpdateMode)
BRIDGE_DECL(sceSblUsInformUpdateFinished)
BRIDGE_DECL(sceSblUsInformUpdateOngoing)
BRIDGE_DECL(sceSblUsInformUpdateStarted)
BRIDGE_DECL(sceSblUsInspectSpackage)
BRIDGE_DECL(sceSblUsPowerControl)
BRIDGE_DECL(sceSblUsReleaseBuffer)
BRIDGE_DECL(sceSblUsSetSwInfoBin)
BRIDGE_DECL(sceSblUsSetSwInfoInt)
BRIDGE_DECL(sceSblUsSetSwInfoStr)
BRIDGE_DECL(sceSblUsSetUpdateMode)
BRIDGE_DECL(sceSblUsUpdateSpackage)
BRIDGE_DECL(sceSblUsVerifyPup)
BRIDGE_DECL(sceSblUsVerifyPupAdditionalSign)
BRIDGE_DECL(sceSblUsVerifyPupHeader)
BRIDGE_DECL(sceSblUsVerifyPupSegment)
BRIDGE_DECL(sceSblUsVerifyPupSegmentById)
BRIDGE_DECL(sceSblUsVerifyPupWatermark)
