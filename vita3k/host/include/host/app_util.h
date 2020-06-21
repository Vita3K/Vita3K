#pragma once

#include <mem/ptr.h>
#include <util/types.h>

typedef unsigned int SceAppUtilBootAttribute;
typedef unsigned int SceAppUtilAppEventType;
typedef unsigned int SceAppUtilSaveDataSlotId;
typedef unsigned int SceAppUtilSaveDataSlotStatus;
typedef unsigned int SceAppUtilAppParamId;
typedef unsigned int SceAppUtilBgdlStatusType;

#define SCE_APPUTIL_NP_DRM_ADDCONT_ID_SIZE 17

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

std::string construct_slotparam_path(const unsigned int data);
