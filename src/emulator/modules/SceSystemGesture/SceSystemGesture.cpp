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

#include "SceSystemGesture.h"

EXPORT(int, sceSystemGestureCreateTouchRecognizer) {
    return unimplemented("sceSystemGestureCreateTouchRecognizer");
}

EXPORT(int, sceSystemGestureFinalizePrimitiveTouchRecognizer) {
    return unimplemented("sceSystemGestureFinalizePrimitiveTouchRecognizer");
}

EXPORT(int, sceSystemGestureGetPrimitiveTouchEventByIndex) {
    return unimplemented("sceSystemGestureGetPrimitiveTouchEventByIndex");
}

EXPORT(int, sceSystemGestureGetPrimitiveTouchEventByPrimitiveID) {
    return unimplemented("sceSystemGestureGetPrimitiveTouchEventByPrimitiveID");
}

EXPORT(int, sceSystemGestureGetPrimitiveTouchEvents) {
    return unimplemented("sceSystemGestureGetPrimitiveTouchEvents");
}

EXPORT(int, sceSystemGestureGetPrimitiveTouchEventsCount) {
    return unimplemented("sceSystemGestureGetPrimitiveTouchEventsCount");
}

EXPORT(int, sceSystemGestureGetTouchEventByEventID) {
    return unimplemented("sceSystemGestureGetTouchEventByEventID");
}

EXPORT(int, sceSystemGestureGetTouchEventByIndex) {
    return unimplemented("sceSystemGestureGetTouchEventByIndex");
}

EXPORT(int, sceSystemGestureGetTouchEvents) {
    return unimplemented("sceSystemGestureGetTouchEvents");
}

EXPORT(int, sceSystemGestureGetTouchEventsCount) {
    return unimplemented("sceSystemGestureGetTouchEventsCount");
}

EXPORT(int, sceSystemGestureGetTouchRecognizerInformation) {
    return unimplemented("sceSystemGestureGetTouchRecognizerInformation");
}

EXPORT(int, sceSystemGestureInitializePrimitiveTouchRecognizer) {
    return unimplemented("sceSystemGestureInitializePrimitiveTouchRecognizer");
}

EXPORT(int, sceSystemGestureResetPrimitiveTouchRecognizer) {
    return unimplemented("sceSystemGestureResetPrimitiveTouchRecognizer");
}

EXPORT(int, sceSystemGestureResetTouchRecognizer) {
    return unimplemented("sceSystemGestureResetTouchRecognizer");
}

EXPORT(int, sceSystemGestureUpdatePrimitiveTouchRecognizer) {
    return unimplemented("sceSystemGestureUpdatePrimitiveTouchRecognizer");
}

EXPORT(int, sceSystemGestureUpdateTouchRecognizer) {
    return unimplemented("sceSystemGestureUpdateTouchRecognizer");
}

EXPORT(int, sceSystemGestureUpdateTouchRecognizerRectangle) {
    return unimplemented("sceSystemGestureUpdateTouchRecognizerRectangle");
}

BRIDGE_IMPL(sceSystemGestureCreateTouchRecognizer)
BRIDGE_IMPL(sceSystemGestureFinalizePrimitiveTouchRecognizer)
BRIDGE_IMPL(sceSystemGestureGetPrimitiveTouchEventByIndex)
BRIDGE_IMPL(sceSystemGestureGetPrimitiveTouchEventByPrimitiveID)
BRIDGE_IMPL(sceSystemGestureGetPrimitiveTouchEvents)
BRIDGE_IMPL(sceSystemGestureGetPrimitiveTouchEventsCount)
BRIDGE_IMPL(sceSystemGestureGetTouchEventByEventID)
BRIDGE_IMPL(sceSystemGestureGetTouchEventByIndex)
BRIDGE_IMPL(sceSystemGestureGetTouchEvents)
BRIDGE_IMPL(sceSystemGestureGetTouchEventsCount)
BRIDGE_IMPL(sceSystemGestureGetTouchRecognizerInformation)
BRIDGE_IMPL(sceSystemGestureInitializePrimitiveTouchRecognizer)
BRIDGE_IMPL(sceSystemGestureResetPrimitiveTouchRecognizer)
BRIDGE_IMPL(sceSystemGestureResetTouchRecognizer)
BRIDGE_IMPL(sceSystemGestureUpdatePrimitiveTouchRecognizer)
BRIDGE_IMPL(sceSystemGestureUpdateTouchRecognizer)
BRIDGE_IMPL(sceSystemGestureUpdateTouchRecognizerRectangle)
