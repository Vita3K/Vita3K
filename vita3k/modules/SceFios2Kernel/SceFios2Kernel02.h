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

struct sceFiosKernelOverlayResolveWithRangeSync02_opt {
    Ptr<char> pOutPath;
    SceSize maxPath;
    char loOrderFilter;
    char hiOrderFilter;
    char reserved1;
    char reserved2;
    int reserved3;
    int reserved4;
    int reserved5;
    int reserved6;
};

BRIDGE_DECL(sceFiosKernelOverlayAddForProcess02)
BRIDGE_DECL(sceFiosKernelOverlayGetInfoForProcess02)
BRIDGE_DECL(sceFiosKernelOverlayGetList02)
BRIDGE_DECL(sceFiosKernelOverlayGetRecommendedScheduler02)
BRIDGE_DECL(sceFiosKernelOverlayModifyForProcess02)
BRIDGE_DECL(sceFiosKernelOverlayRemoveForProcess02)
BRIDGE_DECL(sceFiosKernelOverlayResolveSync02)
BRIDGE_DECL(sceFiosKernelOverlayResolveWithRangeSync02)
BRIDGE_DECL(sceFiosKernelOverlayThreadIsDisabled02)
BRIDGE_DECL(sceFiosKernelOverlayThreadSetDisabled02)
