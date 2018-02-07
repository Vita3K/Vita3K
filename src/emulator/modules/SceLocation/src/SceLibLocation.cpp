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

#include <SceLocation/exports.h>

EXPORT(int, sceLocationCancelGetLocation) {
    return unimplemented("sceLocationCancelGetLocation");
}

EXPORT(int, sceLocationClose) {
    return unimplemented("sceLocationClose");
}

EXPORT(int, sceLocationConfirm) {
    return unimplemented("sceLocationConfirm");
}

EXPORT(int, sceLocationConfirmAbort) {
    return unimplemented("sceLocationConfirmAbort");
}

EXPORT(int, sceLocationConfirmGetResult) {
    return unimplemented("sceLocationConfirmGetResult");
}

EXPORT(int, sceLocationConfirmGetStatus) {
    return unimplemented("sceLocationConfirmGetStatus");
}

EXPORT(int, sceLocationDenyApplication) {
    return unimplemented("sceLocationDenyApplication");
}

EXPORT(int, sceLocationGetHeading) {
    return unimplemented("sceLocationGetHeading");
}

EXPORT(int, sceLocationGetLocation) {
    return unimplemented("sceLocationGetLocation");
}

EXPORT(int, sceLocationGetLocationWithTimeout) {
    return unimplemented("sceLocationGetLocationWithTimeout");
}

EXPORT(int, sceLocationGetMethod) {
    return unimplemented("sceLocationGetMethod");
}

EXPORT(int, sceLocationGetPermission) {
    return unimplemented("sceLocationGetPermission");
}

EXPORT(int, sceLocationInit) {
    return unimplemented("sceLocationInit");
}

EXPORT(int, sceLocationOpen) {
    return unimplemented("sceLocationOpen");
}

EXPORT(int, sceLocationReopen) {
    return unimplemented("sceLocationReopen");
}

EXPORT(int, sceLocationSetGpsEmulationFile) {
    return unimplemented("sceLocationSetGpsEmulationFile");
}

EXPORT(int, sceLocationSetThreadParameter) {
    return unimplemented("sceLocationSetThreadParameter");
}

EXPORT(int, sceLocationStartHeadingCallback) {
    return unimplemented("sceLocationStartHeadingCallback");
}

EXPORT(int, sceLocationStartLocationCallback) {
    return unimplemented("sceLocationStartLocationCallback");
}

EXPORT(int, sceLocationStopHeadingCallback) {
    return unimplemented("sceLocationStopHeadingCallback");
}

EXPORT(int, sceLocationStopLocationCallback) {
    return unimplemented("sceLocationStopLocationCallback");
}

EXPORT(int, sceLocationTerm) {
    return unimplemented("sceLocationTerm");
}

BRIDGE_IMPL(sceLocationCancelGetLocation)
BRIDGE_IMPL(sceLocationClose)
BRIDGE_IMPL(sceLocationConfirm)
BRIDGE_IMPL(sceLocationConfirmAbort)
BRIDGE_IMPL(sceLocationConfirmGetResult)
BRIDGE_IMPL(sceLocationConfirmGetStatus)
BRIDGE_IMPL(sceLocationDenyApplication)
BRIDGE_IMPL(sceLocationGetHeading)
BRIDGE_IMPL(sceLocationGetLocation)
BRIDGE_IMPL(sceLocationGetLocationWithTimeout)
BRIDGE_IMPL(sceLocationGetMethod)
BRIDGE_IMPL(sceLocationGetPermission)
BRIDGE_IMPL(sceLocationInit)
BRIDGE_IMPL(sceLocationOpen)
BRIDGE_IMPL(sceLocationReopen)
BRIDGE_IMPL(sceLocationSetGpsEmulationFile)
BRIDGE_IMPL(sceLocationSetThreadParameter)
BRIDGE_IMPL(sceLocationStartHeadingCallback)
BRIDGE_IMPL(sceLocationStartLocationCallback)
BRIDGE_IMPL(sceLocationStopHeadingCallback)
BRIDGE_IMPL(sceLocationStopLocationCallback)
BRIDGE_IMPL(sceLocationTerm)
