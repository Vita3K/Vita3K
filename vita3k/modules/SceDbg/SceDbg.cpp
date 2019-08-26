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

#include "SceDbg.h"

EXPORT(int, sceDbgAssertionHandler) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceDbgLoggingHandler) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceDbgSetBreakOnErrorState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceDbgSetBreakOnWarningState) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceDbgSetMinimumLogLevel) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceDbgAssertionHandler)
BRIDGE_IMPL(sceDbgLoggingHandler)
BRIDGE_IMPL(sceDbgSetBreakOnErrorState)
BRIDGE_IMPL(sceDbgSetBreakOnWarningState)
BRIDGE_IMPL(sceDbgSetMinimumLogLevel)
