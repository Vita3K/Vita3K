#pragma once

#include <mem/ptr.h>

#include <psp2/apputil.h>

namespace emu {

#define SCE_APPUTIL_NP_DRM_ADDCONT_ID_SIZE 17

struct SceAppUtilDrmAddcontId {
    SceChar8 data[SCE_APPUTIL_NP_DRM_ADDCONT_ID_SIZE];
    SceChar8 padding[3];
};

struct SceAppUtilSaveDataSaveItem {
    const Ptr<char> dataPath;
    const Ptr<void> buf;
    uint32_t pad;
    SceOff offset;
    int mode;
    uint8_t reserved[36];
};

struct SceAppUtilSaveDataFile {
    const Ptr<char> filePath;
    Ptr<void> buf;
    SceSize bufSize;
    SceOff offset;
    unsigned int mode;
    unsigned int progDelta;
    uint8_t reserved[32];
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
    Ptr<emu::SceAppUtilSaveDataSlotParam> slotParam;
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
    Ptr<emu::SceAppUtilSaveDataSlotEmptyParam> emptyParam;
};

struct SceAppUtilSaveDataRemoveItem {
    const Ptr<char> dataPath;
    int mode;
    uint8_t reserved[36];
};
} // namespace emu

std::string construct_slotparam_path(const unsigned int data);
