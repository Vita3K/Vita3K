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

#include <emuenv/app_util.h>
#include <module/module.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

enum SceAppMgrErrorCode : uint32_t {
    SCE_APPMGR_ERROR_INVALID_PARAMETER = 0x80800001, //!< Invalid parameter (NULL)
    SCE_APPMGR_ERROR_INVALID_PARAMETER2 = 0x80801000, //!< Invalid parameter (wrong value)
    SCE_APPMGR_ERROR_INVALID_POINTER = 0x80801001, //!< Invalid pointer
    SCE_APPMGR_ERROR_SAVEDATA_SLOT_EXISTS = 0x8080108a, //!< Save data slot already exists
    SCE_APPMGR_ERROR_SAVEDATA_SLOT_NOT_FOUND = 0x8080108b, //!< Save data slot does not exist
    SCE_APPMGR_ERROR_SAVEDATA_INVALID_PARAMETER = 0x8080108e, //!< Invalid save data parameter
    SCE_APPMGR_ERROR_BUSY = 0x80802000, //!< Busy
    SCE_APPMGR_ERROR_APP_NOT_FOUND = 0x80802012, //!< Application not found
    SCE_APPMGR_ERROR_STATE = 0x80802013, //!< Invalid state
    SCE_APPMGR_ERROR_APP_PARAM_READ = 0x80802015, //!< The app param was already read
    SCE_APPMGR_ERROR_NULL_POINTER = 0x80802016, //!< NULL pointer
    SCE_APPMGR_ERROR_INVALID = 0x8080201A, //!< Invalid param
    SCE_APPMGR_ERROR_TOO_LONG_ARGV = 0x8080201D, //!< argv is too long
    SCE_APPMGR_ERROR_INVALID_SELF_PATH = 0x8080201E, //!< Invalid SELF path
    SCE_APPMGR_ERROR_NO_EVENT = 0x80802020, //!< No application event available
    SCE_APPMGR_ERROR_BGM_PORT_BUSY = 0x80803000 //!< BGM port was occupied and could not be secured
};

struct SceAppMgrEvent {
    SceUInt32 event;
    SceInt32 app_id;
    SceUInt8 param[56];
};

static_assert(sizeof(SceAppMgrEvent) == 64);

struct AppMgrEvents {
    // Each session starts with the launch parameter queued, even when it is empty.
    std::atomic_bool launch_param_pending{ true };
};

struct AppMgrLaunchAppByPathArgs {
    Ptr<const char> param;
    SceUInt32 process_attr;
    Ptr<const void> option;
    SceUInt32 reserved[5];
};

static_assert(sizeof(AppMgrLaunchAppByPathArgs) == 32);
static_assert(offsetof(AppMgrLaunchAppByPathArgs, process_attr) == 4);
static_assert(offsetof(AppMgrLaunchAppByPathArgs, option) == 8);

int appmgr_launch_app_by_path(EmuEnvState &emuenv, Ptr<const char> path, Ptr<const char> title_id,
    SceUInt32 budget_size, Ptr<const char> param, SceUInt32 process_attr, Ptr<const void> option);

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
    SceSize length;
    int32_t reserved[3];
} sceAppMgrAppParamGetStringOptParam;

static_assert(sizeof(sceAppMgrAppParamGetStringOptParam) == 0x10);

struct SceAppMgrDrmOpenParam {
    uint32_t size;
    char dir_name[20];
    char mount_point[16];
};

static_assert(sizeof(SceAppMgrDrmOpenParam) == 0x28);

struct SceAppMgrSaveDataSlot {
    uint32_t size;
    uint32_t slot_id;
    SceAppUtilSaveDataSlotParam param;
    uint8_t reserved[116];
    SceAppUtilMountPoint mount_point;
    uint8_t reserved2[64];
};

struct SceAppMgrSaveDataSlotDelete {
    uint32_t size;
    uint32_t slot_id;
    SceAppUtilMountPoint mount_point;
};

struct SceAppMgrSaveDataQuota {
    uint32_t size;
    SceSize quota_size_kib;
    SceSize used_size_kib;
    SceAppUtilMountPoint mount_point;
};

struct SceAppMgrSaveDataData {
    uint32_t size;
    uint32_t slot_id;
    Ptr<SceAppUtilSaveDataSlotParam> slot_param;
    uint8_t reserved[32];
    Ptr<SceAppUtilSaveDataDataSaveItem> files;
    uint32_t file_count;
    SceAppUtilMountPoint mount_point;
    uint32_t reserved2;
    uint32_t required_size_kib;
};

struct SceAppMgrSaveDataDataDelete {
    uint32_t size;
    uint32_t slot_id;
    Ptr<SceAppUtilSaveDataSlotParam> slot_param;
    uint8_t reserved[32];
    Ptr<SceAppUtilSaveDataRemoveItem> files;
    uint32_t file_count;
    SceAppUtilMountPoint mount_point;
};

static_assert(sizeof(SceAppMgrSaveDataSlot) == 0x418);
static_assert(sizeof(SceAppMgrSaveDataSlotDelete) == 0x18);
static_assert(sizeof(SceAppMgrSaveDataData) == 0x4c);
static_assert(sizeof(SceAppMgrSaveDataDataDelete) == 0x44);
static_assert(sizeof(SceAppMgrSaveDataQuota) == 0x1c);
static_assert(offsetof(SceAppMgrSaveDataData, files) == 0x2c);
static_assert(offsetof(SceAppMgrSaveDataData, required_size_kib) == 0x48);

int execute_savedata_data_save(EmuEnvState &emuenv, SceAppUtilSaveDataFileSlot *slot,
    SceAppUtilSaveDataDataSaveItem *files, uint32_t file_count, SceAppUtilMountPoint *mount_point,
    SceSize *required_size_kib, const char *export_name);
int execute_savedata_data_remove(EmuEnvState &emuenv, SceAppUtilSaveDataFileSlot *slot,
    SceAppUtilSaveDataRemoveItem *files, uint32_t file_count, SceAppUtilMountPoint *mount_point,
    const char *export_name);
SceInt32 execute_savedata_safe_memory(EmuEnvState &emuenv, void *buffer, SceSize size, SceOff offset, const char *export_name, bool save);

struct AppMgrBgmState {
    int32_t bgmOutPortOwner;
    int32_t priority;
    int32_t subPriority;
    int32_t appId;
    uint32_t reserved[4];
};

static_assert(sizeof(AppMgrBgmState) == 0x20);
static_assert(offsetof(AppMgrBgmState, priority) == 0x04);
static_assert(offsetof(AppMgrBgmState, subPriority) == 0x08);
static_assert(offsetof(AppMgrBgmState, appId) == 0x0c);

AppMgrBgmState get_appmgr_bgm_state(EmuEnvState &emuenv);

DECL_EXPORT(SceInt32, __sceAppMgrGetAppState, SceAppMgrAppState *appState, SceUInt32 sizeofSceAppMgrAppState, SceUInt32 buildVersion);
DECL_EXPORT(int, sceAppMgrGetCurrentBgmState2, AppMgrBgmState *state);
DECL_EXPORT(int, sceAppMgrAcquireBgmPortWithPriority, int priority);
DECL_EXPORT(int, sceAppMgrSetBgmSubPriority, int enabled, int priority);
DECL_EXPORT(int, _sceAppMgrAppDataMountById, int mountId, Ptr<const char> id, Ptr<char> mountPoint);
DECL_EXPORT(int, _sceAppMgrGameDataMount, Ptr<const char> source, Ptr<const char> patch, Ptr<const void> license, Ptr<char> mountPoint);
DECL_EXPORT(int, _sceAppMgrThemeDataMount, Ptr<const char> contentId, Ptr<const void> option1, Ptr<const void> option2, Ptr<char> mountPoint);
DECL_EXPORT(int, _sceAppMgrDrmOpen, const SceAppMgrDrmOpenParam *param);
DECL_EXPORT(int, _sceAppMgrDrmClose, const SceAppMgrDrmOpenParam *param);
DECL_EXPORT(int, sceAppMgrDrmOpen, const SceAppMgrDrmOpenParam *param);
DECL_EXPORT(int, sceAppMgrDrmClose, const SceAppMgrDrmOpenParam *param);
DECL_EXPORT(int, sceAppMgrSaveDataSlotCreate, SceAppMgrSaveDataSlot *data);
DECL_EXPORT(int, sceAppMgrSaveDataSlotDelete, SceAppMgrSaveDataSlotDelete *data);
DECL_EXPORT(int, sceAppMgrSaveDataSlotGetParam, SceAppMgrSaveDataSlot *data);
DECL_EXPORT(int, sceAppMgrSaveDataSlotSetParam, SceAppMgrSaveDataSlot *data);
DECL_EXPORT(int, sceAppMgrSaveDataDataSave, SceAppMgrSaveDataData *data);
DECL_EXPORT(int, sceAppMgrSaveDataDataRemove, SceAppMgrSaveDataDataDelete *data);
DECL_EXPORT(int, sceAppMgrSaveDataGetQuota, SceAppMgrSaveDataQuota *data);
DECL_EXPORT(SceInt32, sceAppMgrLoadSafeMemory, void *buffer, SceSize size, SceOff offset);
DECL_EXPORT(SceInt32, sceAppMgrSaveSafeMemory, const void *buffer, SceSize size, SceOff offset);
DECL_EXPORT(int, _sceAppMgrUmount, Ptr<const char> mountPoint);
DECL_EXPORT(SceInt32, _sceAppMgrAppParamGetString, int pid, int param, char *string, sceAppMgrAppParamGetStringOptParam *optParam);
DECL_EXPORT(SceInt32, _sceAppMgrGetAppParam, char *param);
DECL_EXPORT(int, _sceAppMgrReceiveEvent, SceAppMgrEvent *event);
DECL_EXPORT(int, _sceAppMgrReceiveEventNum, SceUInt32 *eventNum);
DECL_EXPORT(SceInt32, _sceAppMgrLoadExec, const char *appPath, Ptr<char> const argv[], const SceAppMgrLoadExecOptParam *optParam);
DECL_EXPORT(SceInt32, _sceAppMgrMmsMount, SceInt32 id, char *mount_point);

SceInt32 __sceAppMgrGetAppState(SceAppMgrAppState *appState, SceUInt32 sizeofSceAppMgrAppState, SceUInt32 buildVersion);
SceInt32 _sceAppMgrLoadExec(const char *appPath, char *const argv[], const SceAppMgrLoadExecOptParam *optParam);
