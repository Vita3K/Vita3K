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

BRIDGE_DECL(sceSystemGestureCreateTouchRecognizer)
BRIDGE_DECL(sceSystemGestureFinalizePrimitiveTouchRecognizer)
BRIDGE_DECL(sceSystemGestureGetPrimitiveTouchEventByIndex)
BRIDGE_DECL(sceSystemGestureGetPrimitiveTouchEventByPrimitiveID)
BRIDGE_DECL(sceSystemGestureGetPrimitiveTouchEvents)
BRIDGE_DECL(sceSystemGestureGetPrimitiveTouchEventsCount)
BRIDGE_DECL(sceSystemGestureGetTouchEventByEventID)
BRIDGE_DECL(sceSystemGestureGetTouchEventByIndex)
BRIDGE_DECL(sceSystemGestureGetTouchEvents)
BRIDGE_DECL(sceSystemGestureGetTouchEventsCount)
BRIDGE_DECL(sceSystemGestureGetTouchRecognizerInformation)
BRIDGE_DECL(sceSystemGestureInitializePrimitiveTouchRecognizer)
BRIDGE_DECL(sceSystemGestureResetPrimitiveTouchRecognizer)
BRIDGE_DECL(sceSystemGestureResetTouchRecognizer)
BRIDGE_DECL(sceSystemGestureUpdatePrimitiveTouchRecognizer)
BRIDGE_DECL(sceSystemGestureUpdateTouchRecognizer)
BRIDGE_DECL(sceSystemGestureUpdateTouchRecognizerRectangle)
