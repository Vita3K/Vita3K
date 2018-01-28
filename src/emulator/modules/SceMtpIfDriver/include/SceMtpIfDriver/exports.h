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

// SceMtpIf
BRIDGE_DECL(sceMtpIfCancelTransfer)
BRIDGE_DECL(sceMtpIfChangePhase)
BRIDGE_DECL(sceMtpIfGetPort)
BRIDGE_DECL(sceMtpIfIsConnected)
BRIDGE_DECL(sceMtpIfRecvCommand)
BRIDGE_DECL(sceMtpIfRecvDataWithParam)
BRIDGE_DECL(sceMtpIfReset)
BRIDGE_DECL(sceMtpIfSendDataWithParam)
BRIDGE_DECL(sceMtpIfSendEvent)
BRIDGE_DECL(sceMtpIfSendResponse)
BRIDGE_DECL(sceMtpIfStartDriver)
BRIDGE_DECL(sceMtpIfStartPort)
BRIDGE_DECL(sceMtpIfStopDriver)
BRIDGE_DECL(sceMtpIfStopPort)
BRIDGE_DECL(sceMtpIfWaitConnect)
