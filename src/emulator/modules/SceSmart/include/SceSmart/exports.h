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

// SceSmart
BRIDGE_DECL(sceSmartCreateLearnedImageTarget)
BRIDGE_DECL(sceSmartDestroyTarget)
BRIDGE_DECL(sceSmartGetTargetInfo)
BRIDGE_DECL(sceSmartInit)
BRIDGE_DECL(sceSmartRelease)
BRIDGE_DECL(sceSmartSceneMappingDispatchAndQuery)
BRIDGE_DECL(sceSmartSceneMappingDispatchAndQueryWithMask)
BRIDGE_DECL(sceSmartSceneMappingEnableMask)
BRIDGE_DECL(sceSmartSceneMappingFixMap)
BRIDGE_DECL(sceSmartSceneMappingForceLocalize)
BRIDGE_DECL(sceSmartSceneMappingGetInitializationPointInfo)
BRIDGE_DECL(sceSmartSceneMappingGetLandmarkInfo)
BRIDGE_DECL(sceSmartSceneMappingGetNodePointInfo)
BRIDGE_DECL(sceSmartSceneMappingGetNumInitializationPoints)
BRIDGE_DECL(sceSmartSceneMappingGetNumLandmarks)
BRIDGE_DECL(sceSmartSceneMappingGetNumNodePoints)
BRIDGE_DECL(sceSmartSceneMappingLoadMap)
BRIDGE_DECL(sceSmartSceneMappingPropagateResult)
BRIDGE_DECL(sceSmartSceneMappingRegisterTarget)
BRIDGE_DECL(sceSmartSceneMappingRemoveLandmark)
BRIDGE_DECL(sceSmartSceneMappingReset)
BRIDGE_DECL(sceSmartSceneMappingRun)
BRIDGE_DECL(sceSmartSceneMappingRunCore)
BRIDGE_DECL(sceSmartSceneMappingSaveMap)
BRIDGE_DECL(sceSmartSceneMappingSetDenseMapMode)
BRIDGE_DECL(sceSmartSceneMappingStart)
BRIDGE_DECL(sceSmartSceneMappingStop)
BRIDGE_DECL(sceSmartSceneMappingUnregisterTarget)
BRIDGE_DECL(sceSmartTargetTrackingDispatchAndQuery)
BRIDGE_DECL(sceSmartTargetTrackingGetResults)
BRIDGE_DECL(sceSmartTargetTrackingQuery)
BRIDGE_DECL(sceSmartTargetTrackingRegisterTarget)
BRIDGE_DECL(sceSmartTargetTrackingReset)
BRIDGE_DECL(sceSmartTargetTrackingRun)
BRIDGE_DECL(sceSmartTargetTrackingRun2)
BRIDGE_DECL(sceSmartTargetTrackingRunWorker)
BRIDGE_DECL(sceSmartTargetTrackingSetSearchPolicy)
BRIDGE_DECL(sceSmartTargetTrackingStart)
BRIDGE_DECL(sceSmartTargetTrackingStop)
BRIDGE_DECL(sceSmartTargetTrackingUnregisterTarget)
