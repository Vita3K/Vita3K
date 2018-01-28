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

#include <SceMtpIfDriver/exports.h>

EXPORT(int, sceMtpIfCancelTransfer) {
    return unimplemented("sceMtpIfCancelTransfer");
}

EXPORT(int, sceMtpIfChangePhase) {
    return unimplemented("sceMtpIfChangePhase");
}

EXPORT(int, sceMtpIfGetPort) {
    return unimplemented("sceMtpIfGetPort");
}

EXPORT(int, sceMtpIfIsConnected) {
    return unimplemented("sceMtpIfIsConnected");
}

EXPORT(int, sceMtpIfRecvCommand) {
    return unimplemented("sceMtpIfRecvCommand");
}

EXPORT(int, sceMtpIfRecvDataWithParam) {
    return unimplemented("sceMtpIfRecvDataWithParam");
}

EXPORT(int, sceMtpIfReset) {
    return unimplemented("sceMtpIfReset");
}

EXPORT(int, sceMtpIfSendDataWithParam) {
    return unimplemented("sceMtpIfSendDataWithParam");
}

EXPORT(int, sceMtpIfSendEvent) {
    return unimplemented("sceMtpIfSendEvent");
}

EXPORT(int, sceMtpIfSendResponse) {
    return unimplemented("sceMtpIfSendResponse");
}

EXPORT(int, sceMtpIfStartDriver) {
    return unimplemented("sceMtpIfStartDriver");
}

EXPORT(int, sceMtpIfStartPort) {
    return unimplemented("sceMtpIfStartPort");
}

EXPORT(int, sceMtpIfStopDriver) {
    return unimplemented("sceMtpIfStopDriver");
}

EXPORT(int, sceMtpIfStopPort) {
    return unimplemented("sceMtpIfStopPort");
}

EXPORT(int, sceMtpIfWaitConnect) {
    return unimplemented("sceMtpIfWaitConnect");
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
