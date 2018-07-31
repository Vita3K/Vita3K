#pragma once

#include <mem/ptr.h>

#include <array>
#include <map>
#include <mutex>
#include <tuple>

#include <psp2/apputil.h>

namespace emu {

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

struct SceAppUtilSaveDataFileSlot {
    unsigned int id;
    Ptr<SceAppUtilSaveDataSlotParam> slotParam;
    uint8_t reserved[32];
};

struct SceAppUtilSaveDataRemoveItem {
    const Ptr<char> dataPath;
    int mode;
    uint8_t reserved[36];
};
}; // namespace emu
