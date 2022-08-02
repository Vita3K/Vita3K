// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

/**
 * @file SceTouch_tracy.h
 * @brief Tracy argument serializing functions for SceTouch
 */

#pragma once

#ifdef TRACY_ENABLE

#include "SceTouch.h"
#include "Tracy.hpp"
#include "client/TracyScoped.hpp"

#include "touch/touch.h"

/**
 * @brief Module name used for retrieving Tracy activation state for logging using Tracy profiler
 *
 * This constant is used by functions in module to know which module name they have
 * to use as a key to retrieve the activation state for logging of the module towards the Tracy profiler.
 */
const std::string tracy_module_name = "SceTouch";

void tracy_sceTouchGetPanelInfo(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchPanelInfo *pPanelInfo);
void tracy_sceTouchGetSamplingState(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchSamplingState *pState);
void tracy_sceTouchPeek(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs);
void tracy_sceTouchPeek2(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs);
void tracy_sceTouchRead(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs);
void tracy_sceTouchRead2(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs);
void tracy_sceTouchSetSamplingState(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchSamplingState state);

#endif // TRACY_ENABLE
