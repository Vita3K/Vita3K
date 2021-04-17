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

struct SceAppUtilSaveDataMountPoint {
    uint8_t data[16];
};

enum SceSystemParamId {
    SCE_SYSTEM_PARAM_ID_LANG = 1,
    SCE_SYSTEM_PARAM_ID_ENTER_BUTTON,
    SCE_SYSTEM_PARAM_ID_USERNAME,
    SCE_SYSTEM_PARAM_ID_DATE_FORMAT,
    SCE_SYSTEM_PARAM_ID_TIME_FORMAT,
    SCE_SYSTEM_PARAM_ID_TIME_ZONE,
    SCE_SYSTEM_PARAM_ID_DAYLIGHT_SAVINGS,
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
