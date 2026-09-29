// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <kernel/types.h>
#include <module/module.h>

struct SceKernelModuleInfo;
struct SceKernelMsgPipeVector;

DECL_EXPORT(SceInt32, sceKernelGetThreadCurrentPriority);
DECL_EXPORT(int, sceKernelGetModuleInfoByAddr, Ptr<void> addr, SceKernelModuleInfo *info);
DECL_EXPORT(int, sceClibPrintf, const char *fmt, module::vargs args);
DECL_EXPORT(SceInt32, sceKernelReceiveMsgPipeVector, SceUID msgPipeId, Ptr<const SceKernelMsgPipeVector> recvVectors, SceUInt32 vectorCount, SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout);
DECL_EXPORT(SceInt32, sceKernelReceiveMsgPipeVectorCB, SceUID msgPipeId, Ptr<const SceKernelMsgPipeVector> recvVectors, SceUInt32 vectorCount, SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout);
DECL_EXPORT(SceInt32, sceKernelSendMsgPipeVector, SceUID msgPipeId, Ptr<const SceKernelMsgPipeVector> sendVectors, SceUInt32 vectorCount, SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout);
DECL_EXPORT(SceInt32, sceKernelSendMsgPipeVectorCB, SceUID msgPipeId, Ptr<const SceKernelMsgPipeVector> sendVectors, SceUInt32 vectorCount, SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout);
