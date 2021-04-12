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

BRIDGE_DECL(ksceFiosKernelOverlayAdd)
BRIDGE_DECL(ksceFiosKernelOverlayAddForProcess)
BRIDGE_DECL(ksceFiosKernelOverlayGetInfo)
BRIDGE_DECL(ksceFiosKernelOverlayGetInfoForProcess)
BRIDGE_DECL(ksceFiosKernelOverlayGetList)
BRIDGE_DECL(ksceFiosKernelOverlayGetRecommendedScheduler)
BRIDGE_DECL(ksceFiosKernelOverlayModify)
BRIDGE_DECL(ksceFiosKernelOverlayModifyForProcess)
BRIDGE_DECL(ksceFiosKernelOverlayRemove)
BRIDGE_DECL(ksceFiosKernelOverlayRemoveForProcess)
BRIDGE_DECL(ksceFiosKernelOverlayResolveSync)
BRIDGE_DECL(ksceFiosKernelOverlayResolveWithRangeSync)
BRIDGE_DECL(ksceFiosKernelOverlayThreadIsDisabled)
BRIDGE_DECL(ksceFiosKernelOverlayThreadSetDisabled)
