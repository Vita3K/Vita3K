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

#include <mem/ptr.h>
#include <util/types.h>

typedef unsigned int SceAppUtilBootAttribute;
typedef unsigned int SceAppUtilAppEventType;
typedef unsigned int SceAppUtilSaveDataSlotId;
typedef unsigned int SceAppUtilSaveDataSlotStatus;
typedef unsigned int SceAppUtilAppParamId;
typedef unsigned int SceAppUtilBgdlStatusType;

#define SCE_APPUTIL_APPPARAM_ID_SKU_FLAG 0
#define SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE 16
#define SCE_APPUTIL_NP_DRM_ADDCONT_ID_SIZE 17
#define SCE_SYSTEM_PARAM_USERNAME_MAXSIZE 17

struct SceAppUtilDrmAddcontId {
    SceChar8 data[SCE_APPUTIL_NP_DRM_ADDCONT_ID_SIZE];
    SceChar8 padding[3];
};

struct SceAppUtilSaveDataDataSaveItem {
    const Ptr<char> dataPath;
    const Ptr<void> buf;
    SceSize bufSize;
    uint32_t pad;
    SceOff offset;
    int mode;
    uint8_t reserved[36];
};

struct SceAppUtilSaveDataSlotParam {
    SceAppUtilSaveDataSlotStatus status;
    char title[64];
    char subTitle[128];
    char detail[512];
    char iconPath[64];
    int userParam;
    SceSize sizeKB;
    SceDateTime modifiedTime;
    uint8_t reserved[48];
};

struct SceAppUtilSaveDataFileSlot {
    unsigned int id;
    Ptr<SceAppUtilSaveDataSlotParam> slotParam;
    uint8_t reserved[32];
};

struct SceAppUtilSaveDataSlotEmptyParam {
    Ptr<char> title;
    Ptr<char> iconPath;
    Ptr<void> iconBuf;
    SceSize iconBufSize;
    uint8_t reserved[32];
};

struct SceAppUtilSaveDataSlot {
    SceAppUtilSaveDataSlotId id;
    SceAppUtilSaveDataSlotStatus status;
    int userParam;
    Ptr<SceAppUtilSaveDataSlotEmptyParam> emptyParam;
};

struct SceAppUtilSaveDataRemoveItem {
    const Ptr<char> dataPath;
    int mode;
    uint8_t reserved[36];
};

enum SceAppUtilSaveDataSlotSearchType {
    SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EXIST_SLOT = 0,
    SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EMPTY_SLOT = 1
};

enum SceAppUtilSaveDataSlotSortKey {
    SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_SLOT_ID = 0,
    SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_USER_PARAM = 1,
    SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_SIZE_KIB = 2,
    SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_MODIFIED_TIME = 3
};

enum SceAppUtilSaveDataSlotSortType {
    SCE_APPUTIL_SAVEDATA_SLOT_SORT_TYPE_ASCENT = 0,
    SCE_APPUTIL_SAVEDATA_SLOT_SORT_TYPE_DESCENT = 1
};

struct SceAppUtilWorkBuffer {
    Ptr<void> buf;
    SceUInt32 bufSize;
    SceChar8 reserved[32];
};

struct SceAppUtilSaveDataSlotSearchCond {
    SceAppUtilSaveDataSlotSearchType type;
    SceAppUtilSaveDataSlotId from;
    SceUInt32 range;
    SceAppUtilSaveDataSlotSortKey key;
    SceAppUtilSaveDataSlotSortType order;
    SceChar8 reserved[32];
};

struct SceAppUtilSlotSearchResult {
    SceUInt32 hitNum;
    Ptr<SceAppUtilSaveDataSlot> slotList;
    SceChar8 reserved[32];
};

struct SceAppUtilMountPoint {
    SceChar8 data[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
};

enum SceSystemParamId {
    SCE_SYSTEM_PARAM_ID_LANG = 1,
    SCE_SYSTEM_PARAM_ID_ENTER_BUTTON,
    SCE_SYSTEM_PARAM_ID_USER_NAME,
    SCE_SYSTEM_PARAM_ID_DATE_FORMAT,
    SCE_SYSTEM_PARAM_ID_TIME_FORMAT,
    SCE_SYSTEM_PARAM_ID_TIME_ZONE,
    SCE_SYSTEM_PARAM_ID_SUMMERTIME,
    SCE_SYSTEM_PARAM_ID_MAX_VALUE = 0xFFFFFFFF
};

enum SceAppUtilSaveDataRemoveMode {
    SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT = 0,
    SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_NO_SLOT = 1
};

enum SceAppUtilSaveDataSaveMode {
    SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE = 0,
    SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE_TRUNCATE = 1,
    SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_DIRECTORY = 2
};

enum SceAppUtilErrorCode {
    SCE_APPUTIL_ERROR_PARAMETER = 0x80100600,
    SCE_APPUTIL_ERROR_NOT_INITIALIZED = 0x80100601,
    SCE_APPUTIL_ERROR_NO_MEMORY = 0x80100602,
    SCE_APPUTIL_ERROR_BUSY = 0x80100603,
    SCE_APPUTIL_ERROR_NOT_MOUNTED = 0x80100604,
    SCE_APPUTIL_ERROR_NO_PERMISSION = 0x80100605,
    SCE_APPUTIL_ERROR_PASSCODE_MISMATCH = 0x80100606,
    SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA = 0x80100620,
    SCE_APPUTIL_ERROR_SAVEDATA_SLOT_EXISTS = 0x80100640,
    SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND = 0x80100641,
    SCE_APPUTIL_ERROR_SAVEDATA_NO_SPACE_QUOTA = 0x80100642,
    SCE_APPUTIL_ERROR_SAVEDATA_NO_SPACE_FS = 0x80100643,
    SCE_APPUTIL_ERROR_DRM_NO_ENTITLEMENT = 0x80100660,
    SCE_APPUTIL_ERROR_PHOTO_DEVICE_NOT_FOUND = 0x80100680,
    SCE_APPUTIL_ERROR_MUSIC_DEVICE_NOT_FOUND = 0x80100685,
    SCE_APPUTIL_ERROR_MOUNT_LIMIT_OVER = 0x80100686,
    SCE_APPUTIL_ERROR_STACKSIZE_TOO_SHORT = 0x801006A0
};

std::string construct_savedata0_path(const std::string &data, const char *ext = "");
std::string construct_slotparam_path(const unsigned int data);
