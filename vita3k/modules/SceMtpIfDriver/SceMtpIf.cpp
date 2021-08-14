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

#include "SceMtpIf.h"

EXPORT(int, sceMtpIfCancelTransfer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfChangePhase) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfGetPort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfIsConnected) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfRecvCommand) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfRecvDataWithParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfReset) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfSendDataWithParam) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfSendEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfSendResponse) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfStartDriver) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfStartPort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfStopDriver) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfStopPort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceMtpIfWaitConnect) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceMtpIfCancelTransfer)
BRIDGE_IMPL(sceMtpIfChangePhase)
BRIDGE_IMPL(sceMtpIfGetPort)
BRIDGE_IMPL(sceMtpIfIsConnected)
BRIDGE_IMPL(sceMtpIfRecvCommand)
BRIDGE_IMPL(sceMtpIfRecvDataWithParam)
BRIDGE_IMPL(sceMtpIfReset)
BRIDGE_IMPL(sceMtpIfSendDataWithParam)
BRIDGE_IMPL(sceMtpIfSendEvent)
BRIDGE_IMPL(sceMtpIfSendResponse)
BRIDGE_IMPL(sceMtpIfStartDriver)
BRIDGE_IMPL(sceMtpIfStartPort)
BRIDGE_IMPL(sceMtpIfStopDriver)
BRIDGE_IMPL(sceMtpIfStopPort)
BRIDGE_IMPL(sceMtpIfWaitConnect)
