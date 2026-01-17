// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

enum SceAppMgrErrorCode : uint32_t {
    SCE_APPMGR_ERROR_INVALID_PARAMETER = 0x80800001, //!< Invalid parameter (NULL)
    SCE_APPMGR_ERROR_INVALID_PARAMETER2 = 0x80801000, //!< Invalid parameter (wrong value)
    SCE_APPMGR_ERROR_BUSY = 0x80802000, //!< Busy
    SCE_APPMGR_ERROR_STATE = 0x80802013, //!< Invalid state
    SCE_APPMGR_ERROR_NULL_POINTER = 0x80802016, //!< NULL pointer
    SCE_APPMGR_ERROR_INVALID = 0x8080201A, //!< Invalid param
    SCE_APPMGR_ERROR_TOO_LONG_ARGV = 0x8080201D, //!< argv is too long
    SCE_APPMGR_ERROR_INVALID_SELF_PATH = 0x8080201E, //!< Invalid SELF path
    SCE_APPMGR_ERROR_BGM_PORT_BUSY = 0x80803000 //!< BGM port was occupied and could not be secured
};

enum SceAppMgrEventType {
    SCE_APPMGR_EVENT_ON_ACTIVATE = 0x10000001, //!< Application activated
    SCE_APPMGR_EVENT_ON_DEACTIVATE = 0x10000002, //!< Application deactivated
    SCE_APPMGR_EVENT_ON_RESUME = 0x10000003, //!< Application resumed
    SCE_APPMGR_EVENT_REQUEST_QUIT = 0x20000001, //!< Application quit request
    SCE_APPMGR_EVENT_PARAM_NOTIFY = 0x20000004, //!< Application parameter notification
    SCE_APPMGR_EVENT_RECOMMENDED_ORIENTATION_CHANGED = 0x20000010 //!< Recommended orientation changed
};

typedef struct SceAppMgrEvent {
    SceInt32 mgrEvent; //!< event ID
    SceUInt8 reserved[60]; //!< Reserved data
} SceAppMgrEvent;

enum SceAppMgrSystemEventType {
    SCE_APPMGR_SYSTEMEVENT_ON_RESUME = 0x10000003, //!< Application resumed
    SCE_APPMGR_SYSTEMEVENT_ON_STORE_PURCHASE = 0x10000004, //!< Store checkout event arrived
    SCE_APPMGR_SYSTEMEVENT_ON_NP_MESSAGE_ARRIVED = 0x10000005, //!< NP message event arrived
    SCE_APPMGR_SYSTEMEVENT_ON_STORE_REDEMPTION = 0x10000006 //!< Promotion code redeemed at PlayStation�Store
};

typedef struct SceAppMgrSystemEvent {
    SceInt32 systemEvent; //!< System event ID
    SceUInt8 reserved[60]; //!< Reserved data
} SceAppMgrSystemEvent;

typedef struct SceAppMgrAppState {
    SceUInt32 systemEventNum; //!< Number of system events
    SceUInt32 appEventNum; //!< Number of application events
    SceBool isSystemUiOverlaid; //!< Truth-value of UI overlaid of system software
    SceUInt8 reserved[128 - sizeof(SceUInt32) * 2 - sizeof(SceBool)]; //!< Reserved area
} SceAppMgrAppState;

typedef struct SceAppMgrLoadExecOptParam {
    int reserved[256 / 4]; //!< Reserved area
} SceAppMgrLoadExecOptParam;

typedef struct sceAppMgrAppParamGetStringOptParam {
    SceSize size;
    int32_t unk_4;
    int32_t unk_8;
    int32_t unk_C;
} sceAppMgrAppParamGetStringOptParam;

DECL_EXPORT(SceInt32, __sceAppMgrGetAppState, SceAppMgrAppState *appState, SceUInt32 sizeofSceAppMgrAppState, SceUInt32 buildVersion);
DECL_EXPORT(SceInt32, _sceAppMgrAppParamGetString, int pid, int param, char *string, sceAppMgrAppParamGetStringOptParam *optParam);
DECL_EXPORT(SceInt32, _sceAppMgrLoadExec, const char *appPath, Ptr<char> const argv[], const SceAppMgrLoadExecOptParam *optParam);
DECL_EXPORT(SceInt32, _sceAppMgrMmsMount, SceInt32 id, char *mount_point);

SceInt32 __sceAppMgrGetAppState(SceAppMgrAppState *appState, SceUInt32 sizeofSceAppMgrAppState, SceUInt32 buildVersion);
SceInt32 _sceAppMgrLoadExec(const char *appPath, char *const argv[], const SceAppMgrLoadExecOptParam *optParam);
