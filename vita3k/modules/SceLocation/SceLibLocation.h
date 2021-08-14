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

BRIDGE_DECL(sceLocationCancelGetLocation)
BRIDGE_DECL(sceLocationClose)
BRIDGE_DECL(sceLocationConfirm)
BRIDGE_DECL(sceLocationConfirmAbort)
BRIDGE_DECL(sceLocationConfirmGetResult)
BRIDGE_DECL(sceLocationConfirmGetStatus)
BRIDGE_DECL(sceLocationDenyApplication)
BRIDGE_DECL(sceLocationGetHeading)
BRIDGE_DECL(sceLocationGetLocation)
BRIDGE_DECL(sceLocationGetLocationWithTimeout)
BRIDGE_DECL(sceLocationGetMethod)
BRIDGE_DECL(sceLocationGetPermission)
BRIDGE_DECL(sceLocationInit)
BRIDGE_DECL(sceLocationOpen)
BRIDGE_DECL(sceLocationReopen)
BRIDGE_DECL(sceLocationSetGpsEmulationFile)
BRIDGE_DECL(sceLocationSetThreadParameter)
BRIDGE_DECL(sceLocationStartHeadingCallback)
BRIDGE_DECL(sceLocationStartLocationCallback)
BRIDGE_DECL(sceLocationStopHeadingCallback)
BRIDGE_DECL(sceLocationStopLocationCallback)
BRIDGE_DECL(sceLocationTerm)
