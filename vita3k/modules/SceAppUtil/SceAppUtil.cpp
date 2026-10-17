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

#include "SceAppUtil.h"
#include "init.h"

#include <emuenv/app_util.h>

#include <io/device.h>
#include <io/functions.h>
#include <io/io.h>
#include <io/vfs.h>
#include <kernel/state.h>
#include <kernel/thread/thread_state.h>

#include <packages/license.h>
#include <packages/sfo.h>

#include "../SceAppMgr/SceAppMgr.h"
#include "../SceProcessmgr/SceProcessmgr.h"

#include <util/fs.h>
#include <util/safe_time.h>
#include <util/tracy.h>

#ifdef _WIN32
#include <winsock.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

TRACY_MODULE_NAME(SceAppUtil);

template <>
std::string to_debug_str<SceSystemParamId>(const MemState &mem, SceSystemParamId type) {
    switch (type) {
    case SCE_SYSTEM_PARAM_ID_LANG:
        return "SCE_SYSTEM_PARAM_ID_LANG";
    case SCE_SYSTEM_PARAM_ID_ENTER_BUTTON:
        return "SCE_SYSTEM_PARAM_ID_ENTER_BUTTON";
    case SCE_SYSTEM_PARAM_ID_USER_NAME:
        return "SCE_SYSTEM_PARAM_ID_USER_NAME";
    case SCE_SYSTEM_PARAM_ID_DATE_FORMAT:
        return "SCE_SYSTEM_PARAM_ID_DATE_FORMAT";
    case SCE_SYSTEM_PARAM_ID_TIME_FORMAT:
        return "SCE_SYSTEM_PARAM_ID_TIME_FORMAT";
    case SCE_SYSTEM_PARAM_ID_TIME_ZONE:
        return "SCE_SYSTEM_PARAM_ID_TIME_ZONE";
    case SCE_SYSTEM_PARAM_ID_SUMMERTIME:
        return "SCE_SYSTEM_PARAM_ID_SUMMERTIME";
    case SCE_SYSTEM_PARAM_ID_MAX_VALUE:
        return "SCE_SYSTEM_PARAM_ID_MAX_VALUE";
    }
    return std::to_string(type);
}

EXPORT(int, sceAppUtilAddCookieWebBrowser) {
    TRACY_FUNC(sceAppUtilAddCookieWebBrowser);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAddcontMount) {
    TRACY_FUNC(sceAppUtilAddcontMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAddcontUmount) {
    TRACY_FUNC(sceAppUtilAddcontUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseGameCustomData) {
    TRACY_FUNC(sceAppUtilAppEventParseGameCustomData);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseIncomingDialog) {
    TRACY_FUNC(sceAppUtilAppEventParseIncomingDialog);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseLiveArea) {
    TRACY_FUNC(sceAppUtilAppEventParseLiveArea);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNearGift) {
    TRACY_FUNC(sceAppUtilAppEventParseNearGift);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNpActivity) {
    TRACY_FUNC(sceAppUtilAppEventParseNpActivity);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNpAppDataMessage) {
    TRACY_FUNC(sceAppUtilAppEventParseNpAppDataMessage);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNpBasicJoinablePresence) {
    TRACY_FUNC(sceAppUtilAppEventParseNpBasicJoinablePresence);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseNpInviteMessage) {
    TRACY_FUNC(sceAppUtilAppEventParseNpInviteMessage);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseScreenShotNotification) {
    TRACY_FUNC(sceAppUtilAppEventParseScreenShotNotification);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseSessionInvitation) {
    TRACY_FUNC(sceAppUtilAppEventParseSessionInvitation);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseTeleport) {
    TRACY_FUNC(sceAppUtilAppEventParseTeleport);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseTriggerUtil) {
    TRACY_FUNC(sceAppUtilAppEventParseTriggerUtil);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilAppEventParseWebBrowser) {
    TRACY_FUNC(sceAppUtilAppEventParseWebBrowser);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAppUtilAppParamGetInt, SceAppUtilAppParamId paramId, SceInt32 *value) {
    TRACY_FUNC(sceAppUtilAppParamGetInt, paramId, value);
    if (paramId != SCE_APPUTIL_APPPARAM_ID_SKU_FLAG)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    if (!value)
        return RET_ERROR(SCE_APPUTIL_ERROR_NOT_INITIALIZED);

    *value = emuenv.license.rif[emuenv.io.title_id].sku_flag;

    return 0;
}

EXPORT(int, sceAppUtilBgdlGetStatus) {
    TRACY_FUNC(sceAppUtilBgdlGetStatus);
    return UNIMPLEMENTED();
}

static bool make_drm_param(const SceAppUtilDrmAddcontId *dir_name, const SceAppUtilMountPoint *mount_point, SceAppMgrDrmOpenParam &param) {
    if (!dir_name || dir_name->data[16] != '\0'
        || !std::all_of(dir_name->data, dir_name->data + 16, [](char c) {
               return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
           }))
        return false;
    const char *point = "addcont0:";
    if (mount_point) {
        if (!std::memchr(mount_point->data, '\0', sizeof(mount_point->data)))
            return false;
        point = reinterpret_cast<const char *>(mount_point->data);
        if (std::strcmp(point, "addcont0:") != 0 && std::strcmp(point, "addcont1:") != 0
            && std::strcmp(point, "addcont2:") != 0)
            return false;
    }
    param.size = sizeof(param);
    std::memcpy(param.dir_name, dir_name->data, 16);
    std::strncpy(param.mount_point, point, sizeof(param.mount_point) - 1);
    return true;
}

EXPORT(SceInt32, sceAppUtilDrmClose, const SceAppUtilDrmAddcontId *dirName, const SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilDrmClose, dirName, mountPoint);
    SceAppMgrDrmOpenParam param{};
    if (!make_drm_param(dirName, mountPoint, param))
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    const int result = CALL_EXPORT(sceAppMgrDrmClose, &param);
    if (result == SCE_ERROR_ERRNO_ENOENT || result == static_cast<int>(0x80800002u))
        return RET_ERROR(SCE_APPUTIL_ERROR_NOT_MOUNTED);
    if (result == SCE_ERROR_ERRNO_EBUSY)
        return RET_ERROR(SCE_APPUTIL_ERROR_BUSY);
    return result;
}

EXPORT(SceInt32, sceAppUtilDrmOpen, const SceAppUtilDrmAddcontId *dirName, const SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilDrmOpen, dirName, mountPoint);
    SceAppMgrDrmOpenParam param{};
    if (!make_drm_param(dirName, mountPoint, param))
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    return CALL_EXPORT(sceAppMgrDrmOpen, &param);
}

EXPORT(int, sceAppUtilInit, Ptr<const void> initParam, Ptr<void> bootParam) {
    TRACY_FUNC(sceAppUtilInit, initParam, bootParam);
    const auto thread = emuenv.kernel.get_thread(thread_id);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    auto &mem = thread->cpu->mem;
    if (emuenv.kernel.is_primary_address_space(mem))
        return UNIMPLEMENTED();
    auto *state = find_secondary_app_util_state(emuenv, mem);
    if (!state) {
        LOG_ERROR("sceAppUtilInit cannot find the secondary process");
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }
    const uint32_t sdk_version = static_cast<uint32_t>(CALL_EXPORT(sceKernelGetMainModuleSdkVersion));
    const int result = initialize_secondary_app_util(mem, *state, sdk_version, initParam, bootParam, [&] {
        if (!state->io) {
            LOG_ERROR("sceAppUtilInit cannot initialize slots without a secondary savedata mount");
            return static_cast<int>(SCE_KERNEL_ERROR_NOT_IMPLEMENTED);
        }
        return initialize_savedata_slots(*state->io, emuenv.vita_fs_path);
    });
    if (result == 0)
        LOG_WARN("Secondary AppUtil initialized; other savedata imports are not yet isolated");
    return result;
}

EXPORT(int, sceAppUtilLaunchWebBrowser) {
    TRACY_FUNC(sceAppUtilLaunchWebBrowser);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilMusicMount) {
    TRACY_FUNC(sceAppUtilMusicMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilMusicUmount) {
    TRACY_FUNC(sceAppUtilMusicUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilPhotoMount) {
    TRACY_FUNC(sceAppUtilPhotoMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilPhotoUmount) {
    TRACY_FUNC(sceAppUtilPhotoUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilPspSaveDataGetDirNameList) {
    TRACY_FUNC(sceAppUtilPspSaveDataGetDirNameList);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilPspSaveDataLoad) {
    TRACY_FUNC(sceAppUtilPspSaveDataLoad);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilReceiveAppEvent) {
    TRACY_FUNC(sceAppUtilReceiveAppEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilResetCookieWebBrowser) {
    TRACY_FUNC(sceAppUtilResetCookieWebBrowser);
    return UNIMPLEMENTED();
}

std::string construct_savedata0_path(const std::string &data, const char *ext) {
    return device::construct_normalized_path(VitaIoDevice::savedata0, data, ext);
}

std::string construct_slotparam_path(const unsigned int data) {
    return construct_savedata0_path("SlotParam_" + std::to_string(data), "bin");
}

constexpr size_t slot_flag_base = 0x200;
constexpr size_t slot_data_base = 0x400;
constexpr size_t slot_block_size = 0x400;
constexpr size_t sdslot_size = slot_data_base + SCE_APPUTIL_SAVEDATA_SLOT_MAX * slot_block_size;
constexpr std::array<uint8_t, 10> sdslot_magic{ 'S', 'D', 'S', 'L', 0, 0, 0, 0, 0, 1 };
static_assert(sizeof(SceAppUtilSaveDataSlotParam) == 844);
static std::recursive_mutex sdslot_mutex;
static std::atomic<uint64_t> sdslot_temp_counter{ 0 };

static std::string sdslot_path() {
    return construct_savedata0_path("sce_sys/sdslot", "dat");
}

static std::optional<fs::path> protected_savedata_relative(const char *path) {
    std::string_view relative(path);
    if (relative.starts_with('/'))
        relative.remove_prefix(1);
    if (relative.empty() || relative.starts_with('/') || relative.find_first_of("\\:") != std::string_view::npos)
        return std::nullopt;
    return fs_utils::utf8_to_path(std::string(relative));
}

static SaveDataSlotReadResult read_slot_file(EmuEnvState &emuenv, const std::string &path, void *data, size_t size, const char *export_name) try {
    bool mounted_savedata = false;
    {
        std::lock_guard lock(emuenv.io.appmeta_mutex);
        mounted_savedata = !emuenv.io.savedata_pfs_mount.empty() && path.starts_with("savedata0:");
    }
    if (!mounted_savedata) {
        const auto host_path = expand_path(emuenv.io, path.c_str(), emuenv.vita_fs_path);
        if (host_path.empty()) {
            LOG_ERROR("{}: cannot resolve savedata file {}", export_name, path);
            return SaveDataSlotReadResult::Error;
        }
        std::error_code ec;
        const auto link_status = fs::symlink_status(host_path, ec);
        if (ec == std::errc::no_such_file_or_directory || (!ec && !fs::exists(link_status)))
            return SaveDataSlotReadResult::NotFound;
        if (ec) {
            LOG_ERROR("{}: cannot inspect savedata file {}: {}", export_name, path, ec.message());
            return SaveDataSlotReadResult::Error;
        }
        if (fs::is_symlink(link_status)) {
            LOG_ERROR("{}: refusing to replace or migrate a symbolic-link savedata file {}", export_name, path);
            return SaveDataSlotReadResult::Error;
        }
        const auto status = fs::status(host_path, ec);
        if (ec || !fs::is_regular_file(status)) {
            LOG_ERROR("{}: cannot inspect savedata file {}: {}", export_name, path, ec ? ec.message() : "not a regular file");
            return SaveDataSlotReadResult::Error;
        }
    }
    const auto fd = open_file(emuenv.io, path.c_str(), SCE_O_RDONLY, emuenv.vita_fs_path, export_name, mounted_savedata);
    if (mounted_savedata && fd == SCE_ERROR_ERRNO_ENOENT)
        return SaveDataSlotReadResult::NotFound;
    if (fd < 0) {
        LOG_ERROR("{}: cannot open savedata file {}", export_name, path);
        return SaveDataSlotReadResult::Error;
    }
    const auto file_size = seek_file(fd, 0, SCE_SEEK_END, emuenv.io, export_name);
    const bool valid = file_size == static_cast<SceOff>(size)
        && seek_file(fd, 0, SCE_SEEK_SET, emuenv.io, export_name) == 0
        && read_file(data, emuenv.io, fd, static_cast<SceSize>(size), export_name) == static_cast<int>(size);
    const bool closed = close_file(emuenv.io, fd, export_name) == 0;
    if (!valid || !closed) {
        LOG_ERROR("{}: invalid size or failed to read savedata file {}", export_name, path);
        return SaveDataSlotReadResult::Error;
    }
    return SaveDataSlotReadResult::Found;
} catch (const fs::filesystem_error &error) {
    LOG_ERROR("{}: cannot read savedata file {}: {}", export_name, path, error.what());
    return SaveDataSlotReadResult::Error;
}

static SaveDataSlotReadResult load_sdslot(EmuEnvState &emuenv, std::vector<uint8_t> &data, const char *export_name) {
    data.resize(sdslot_size);
    const auto result = read_slot_file(emuenv, sdslot_path(), data.data(), data.size(), export_name);
    if (result == SaveDataSlotReadResult::Found && !std::equal(sdslot_magic.begin(), sdslot_magic.end(), data.begin())) {
        LOG_ERROR("{}: invalid sdslot.dat magic; leaving the file unchanged", export_name);
        return SaveDataSlotReadResult::Error;
    }
    return result;
}

static SaveDataSlotReadResult read_slot_unlocked(EmuEnvState &emuenv, uint32_t slot_id, SceAppUtilSaveDataSlotParam &param, const char *export_name) {
    if (slot_id >= SCE_APPUTIL_SAVEDATA_SLOT_MAX)
        return SaveDataSlotReadResult::Error;
    std::vector<uint8_t> data;
    const auto result = load_sdslot(emuenv, data, export_name);
    if (result == SaveDataSlotReadResult::NotFound)
        return read_slot_file(emuenv, construct_slotparam_path(slot_id), &param, sizeof(param), export_name);
    if (result == SaveDataSlotReadResult::Error || !data[slot_flag_base + slot_id])
        return result == SaveDataSlotReadResult::Error ? result : SaveDataSlotReadResult::NotFound;
    std::memcpy(&param, data.data() + slot_data_base + slot_id * slot_block_size, sizeof(param));
    return SaveDataSlotReadResult::Found;
}

static bool publish_sdslot(EmuEnvState &emuenv, const std::vector<uint8_t> &data, const char *export_name) try {
    bool protected_savedata;
    {
        const std::lock_guard lock(emuenv.io.appmeta_mutex);
        protected_savedata = !emuenv.io.savedata_pfs_mount.empty();
    }
    if (protected_savedata)
        return write_protected_savedata(emuenv.io, fs::path("sce_sys") / "sdslot.dat", 0, data, data.size()) == 0;
    const auto parent = construct_savedata0_path("sce_sys");
    try {
        create_dir(emuenv.io, parent.c_str(), 0777, emuenv.vita_fs_path, export_name, true);
    } catch (const fs::filesystem_error &error) {
        LOG_ERROR("{}: cannot create sdslot directory: {}", export_name, error.what());
        return false;
    }
    std::string temp_dir;
    for (unsigned int attempt = 0; attempt < 1024; ++attempt) {
        temp_dir = construct_savedata0_path("sce_sys/.vita3k-sdslot-" + std::to_string(sdslot_temp_counter++));
        int result;
        try {
            result = create_dir(emuenv.io, temp_dir.c_str(), 0777, emuenv.vita_fs_path, export_name);
        } catch (const fs::filesystem_error &error) {
            LOG_ERROR("{}: cannot create temporary sdslot directory: {}", export_name, error.what());
            return false;
        }
        if (result == 0)
            break;
        if (result != SCE_ERROR_ERRNO_EEXIST) {
            LOG_ERROR("{}: cannot create temporary sdslot directory", export_name);
            return false;
        }
        temp_dir.clear();
    }
    if (temp_dir.empty()) {
        LOG_ERROR("{}: cannot reserve a temporary sdslot directory", export_name);
        return false;
    }
    const auto temp_file = temp_dir + "/sdslot.dat";
    const auto fd = open_file(emuenv.io, temp_file.c_str(), SCE_O_WRONLY | SCE_O_CREAT, emuenv.vita_fs_path, export_name);
    const bool written = fd >= 0 && write_file(fd, data.data(), static_cast<SceSize>(data.size()), emuenv.io, export_name) == static_cast<int>(data.size());
    const bool closed = fd >= 0 && close_file(emuenv.io, fd, export_name) == 0;
    bool published = false;
    if (written && closed)
        published = rename(emuenv.io, temp_file.c_str(), sdslot_path().c_str(), emuenv.vita_fs_path, export_name) == 0;
    if (!published) {
        LOG_ERROR("{}: failed to publish sdslot.dat; existing files were preserved", export_name);
        if (fd >= 0)
            remove_file(emuenv.io, temp_file.c_str(), emuenv.vita_fs_path, export_name);
    }
    remove_dir(emuenv.io, temp_dir.c_str(), emuenv.vita_fs_path, export_name);
    return published;
} catch (const fs::filesystem_error &error) {
    LOG_ERROR("{}: cannot publish sdslot.dat: {}", export_name, error.what());
    return false;
}

int write_savedata_slot(EmuEnvState &emuenv, uint32_t slot_id, const SceAppUtilSaveDataSlotParam *param, SaveDataSlotWriteMode mode, const SceDateTime *time, const char *export_name) {
    if (slot_id >= SCE_APPUTIL_SAVEDATA_SLOT_MAX)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    const std::lock_guard lock(sdslot_mutex);
    std::vector<uint8_t> data;
    const auto result = load_sdslot(emuenv, data, export_name);
    if (result == SaveDataSlotReadResult::Error)
        return RET_ERROR(SCE_ERROR_ERRNO_EIO);
    if (result == SaveDataSlotReadResult::NotFound) {
        std::copy(sdslot_magic.begin(), sdslot_magic.end(), data.begin());
        for (uint32_t i = 0; i < SCE_APPUTIL_SAVEDATA_SLOT_MAX; ++i) {
            SceAppUtilSaveDataSlotParam legacy{};
            const auto legacy_result = read_slot_file(emuenv, construct_slotparam_path(i), &legacy, sizeof(legacy), export_name);
            if (legacy_result == SaveDataSlotReadResult::Error)
                return RET_ERROR(SCE_ERROR_ERRNO_EIO);
            if (legacy_result == SaveDataSlotReadResult::Found) {
                data[slot_flag_base + i] = 1;
                std::memcpy(data.data() + slot_data_base + i * slot_block_size, &legacy, sizeof(legacy));
            }
        }
    }
    const auto flag = slot_flag_base + slot_id;
    const auto offset = slot_data_base + slot_id * slot_block_size;
    if (mode == SaveDataSlotWriteMode::CreateOnly && data[flag])
        return RET_ERROR(SCE_APPUTIL_ERROR_SAVEDATA_SLOT_EXISTS);
    if ((mode == SaveDataSlotWriteMode::Set || mode == SaveDataSlotWriteMode::DeleteExisting
            || mode == SaveDataSlotWriteMode::UpdateTime)
        && !data[flag])
        return RET_ERROR(SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND);
    if (mode == SaveDataSlotWriteMode::Delete || mode == SaveDataSlotWriteMode::DeleteExisting) {
        data[flag] = 0;
    } else if (mode == SaveDataSlotWriteMode::UpdateTime) {
        SceAppUtilSaveDataSlotParam existing{};
        std::memcpy(&existing, data.data() + offset, sizeof(existing));
        existing.modifiedTime = *time;
        std::memcpy(data.data() + offset, &existing, sizeof(existing));
    } else {
        data[flag] = 1;
        std::memcpy(data.data() + offset, param, sizeof(*param));
    }
    return publish_sdslot(emuenv, data, export_name) ? 0 : RET_ERROR(SCE_ERROR_ERRNO_EIO);
}

SaveDataSlotReadResult read_savedata_slot(EmuEnvState &emuenv, uint32_t slot_id, SceAppUtilSaveDataSlotParam &param, const char *export_name) {
    const std::lock_guard lock(sdslot_mutex);
    return read_slot_unlocked(emuenv, slot_id, param, export_name);
}

int execute_savedata_data_remove(EmuEnvState &emuenv, SceAppUtilSaveDataFileSlot *slot, SceAppUtilSaveDataRemoveItem *files, uint32_t fileNum, SceAppUtilMountPoint *mountPoint, const char *export_name) {
    if (slot && (slot->id >= SCE_APPUTIL_SAVEDATA_SLOT_MAX))
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    if (fileNum && !files)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    bool protected_savedata;
    {
        const std::lock_guard lock(emuenv.io.appmeta_mutex);
        protected_savedata = !emuenv.io.savedata_pfs_mount.empty();
    }
    if (protected_savedata) {
        for (unsigned int i = 0; i < fileNum; ++i) {
            if (!files[i].dataPath)
                return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
            const auto path = protected_savedata_relative(files[i].dataPath.get(emuenv.mem));
            if (!path)
                return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
            const auto result = remove_protected_savedata(emuenv.io, *path);
            if (result != 0)
                return RET_ERROR(result);
        }
        if (slot && fileNum > 0 && files[0].mode == SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT) {
            const auto result = write_savedata_slot(emuenv, slot->id, nullptr, SaveDataSlotWriteMode::Delete, nullptr, export_name);
            return result != 0 ? RET_ERROR(result) : 0;
        }
        return 0;
    }
    if (slot && fileNum > 0 && files[0].mode == SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT) {
        const auto result = write_savedata_slot(emuenv, slot->id, nullptr, SaveDataSlotWriteMode::Delete, nullptr, export_name);
        if (result != 0)
            return RET_ERROR(result);
    }

    for (unsigned int i = 0; i < fileNum; i++) {
        const auto file = fs::path(construct_savedata0_path(files[i].dataPath.get(emuenv.mem)));
        if (fs::is_regular_file(file)) {
            remove_file(emuenv.io, file.string().c_str(), emuenv.vita_fs_path, export_name);
        } else
            remove_dir(emuenv.io, file.string().c_str(), emuenv.vita_fs_path, export_name);
    }

    return 0;
}

int execute_savedata_data_save(EmuEnvState &emuenv, SceAppUtilSaveDataFileSlot *slot, SceAppUtilSaveDataDataSaveItem *files, uint32_t fileNum, SceAppUtilMountPoint *mountPoint, SceSize *requiredSizeKiB, const char *export_name) {
    SceUID fd;

    if (slot && (slot->id >= SCE_APPUTIL_SAVEDATA_SLOT_MAX))
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    if (fileNum && !files)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    std::unique_lock slot_lock(sdslot_mutex, std::defer_lock);
    if (slot)
        slot_lock.lock();
    if (slot && !slot->slotParam) {
        SceAppUtilSaveDataSlotParam existing{};
        const auto result = read_savedata_slot(emuenv, slot->id, existing, export_name);
        if (result == SaveDataSlotReadResult::NotFound)
            return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
        if (result == SaveDataSlotReadResult::Error)
            return RET_ERROR(SCE_ERROR_ERRNO_EIO);
    }
    bool protected_savedata;
    {
        const std::lock_guard lock(emuenv.io.appmeta_mutex);
        protected_savedata = !emuenv.io.savedata_pfs_mount.empty();
    }
    if (requiredSizeKiB)
        // requiredSizeKiB must be set to 0 if there is enough space available
        *requiredSizeKiB = 0;

    for (unsigned int i = 0; i < fileNum; i++) {
        if (protected_savedata) {
            if (!files[i].dataPath || files[i].offset < 0)
                return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
            const auto *guest_path = files[i].dataPath.get(emuenv.mem);
            const auto relative = protected_savedata_relative(guest_path);
            if (!relative)
                return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
            if (files[i].mode == SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_DIRECTORY) {
                const auto result = create_protected_savedata_directories(emuenv.io, *relative);
                if (result != 0)
                    return RET_ERROR(result);
                continue;
            }
            if (files[i].mode != SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE
                && files[i].mode != SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE_TRUNCATE)
                return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
            if (!files[i].buf && files[i].bufSize && files[i].mode == SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE)
                return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
            const auto offset = static_cast<uint64_t>(files[i].offset);
            if (offset > std::numeric_limits<uint64_t>::max() - files[i].bufSize)
                return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
            const auto *buffer = files[i].buf ? static_cast<const uint8_t *>(files[i].buf.get(emuenv.mem)) : nullptr;
            const std::span<const uint8_t> data(buffer, buffer ? files[i].bufSize : 0);
            const auto truncate_size = files[i].mode == SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE_TRUNCATE
                ? std::optional<uint64_t>(offset + files[i].bufSize)
                : std::nullopt;
            if (!relative->parent_path().empty()) {
                const auto result = create_protected_savedata_directories(emuenv.io, relative->parent_path());
                if (result != 0)
                    return RET_ERROR(result);
            }
            const auto result = write_protected_savedata(emuenv.io, *relative, offset, data, truncate_size);
            if (result != 0)
                return RET_ERROR(result);
            continue;
        }
        const auto file_path = construct_savedata0_path(files[i].dataPath.get(emuenv.mem));
        switch (files[i].mode) {
        case SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_DIRECTORY:
            create_dir(emuenv.io, file_path.c_str(), 0777, emuenv.vita_fs_path, export_name);
            break;
        case SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE_TRUNCATE:
            if (files[i].buf) {
                fd = open_file(emuenv.io, file_path.c_str(), SCE_O_WRONLY | SCE_O_CREAT, emuenv.vita_fs_path, export_name);
                seek_file(fd, static_cast<int>(files[i].offset), SCE_SEEK_SET, emuenv.io, export_name);
                write_file(fd, files[i].buf.get(emuenv.mem), files[i].bufSize, emuenv.io, export_name);
                close_file(emuenv.io, fd, export_name);
            }
            fd = open_file(emuenv.io, file_path.c_str(), SCE_O_WRONLY | SCE_O_APPEND | SCE_O_TRUNC, emuenv.vita_fs_path, export_name);
            truncate_file(fd, files[i].bufSize + files[i].offset, emuenv.io, export_name);
            close_file(emuenv.io, fd, export_name);
            break;
        case SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE:
        default:
            fd = open_file(emuenv.io, file_path.c_str(), SCE_O_WRONLY | SCE_O_CREAT, emuenv.vita_fs_path, export_name);
            seek_file(fd, static_cast<int>(files[i].offset), SCE_SEEK_SET, emuenv.io, export_name);
            write_file(fd, files[i].buf.get(emuenv.mem), files[i].bufSize, emuenv.io, export_name);
            close_file(emuenv.io, fd, export_name);
            break;
        }
    }

    if (slot) {
        auto *slot_param = slot->slotParam ? slot->slotParam.get(emuenv.mem) : nullptr;
        SceDateTime modified_time{};
        std::time_t time = std::time(0);
        tm local = {};

        SAFE_LOCALTIME(&time, &local);
        modified_time.year = local.tm_year + 1900;
        modified_time.month = local.tm_mon + 1;
        modified_time.day = local.tm_mday;
        modified_time.hour = local.tm_hour;
        modified_time.minute = local.tm_min;
        modified_time.second = local.tm_sec;
        if (slot_param)
            slot_param->modifiedTime = modified_time;
        const auto result = write_savedata_slot(emuenv, slot->id, slot_param,
            slot_param ? SaveDataSlotWriteMode::Create : SaveDataSlotWriteMode::UpdateTime, &modified_time, export_name);
        if (result != 0 && !(slot_param == nullptr && result == static_cast<int>(SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND)))
            return RET_ERROR(result);
    }

    return 0;
}

EXPORT(int, sceAppUtilSaveDataDataRemove, SceAppUtilSaveDataFileSlot *slot, Ptr<SceAppUtilSaveDataRemoveItem> files, unsigned int fileNum, SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataDataRemove, slot, files, fileNum, mountPoint);
    if ((slot && slot->id >= SCE_APPUTIL_SAVEDATA_SLOT_MAX) || (fileNum && !files))
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    SceAppMgrSaveDataDataDelete data{};
    data.size = sizeof(data);
    data.slot_id = slot ? slot->id : UINT32_MAX;
    if (slot)
        data.slot_param = slot->slotParam;
    data.files = files;
    data.file_count = fileNum;
    if (mountPoint)
        data.mount_point = *mountPoint;
    const auto result = CALL_EXPORT(sceAppMgrSaveDataDataRemove, &data);
    if (result == static_cast<int>(SCE_APPMGR_ERROR_SAVEDATA_INVALID_PARAMETER))
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    return result != 0 ? RET_ERROR(result) : 0;
}

EXPORT(int, sceAppUtilSaveDataDataSave, SceAppUtilSaveDataFileSlot *slot, Ptr<SceAppUtilSaveDataDataSaveItem> files, unsigned int fileNum, SceAppUtilMountPoint *mountPoint, SceSize *requiredSizeKiB) {
    TRACY_FUNC(sceAppUtilSaveDataDataSave, slot, files, fileNum, mountPoint, requiredSizeKiB);
    if ((slot && slot->id >= SCE_APPUTIL_SAVEDATA_SLOT_MAX) || (fileNum && !files))
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    SceAppMgrSaveDataData data{};
    data.size = sizeof(data);
    data.slot_id = slot ? slot->id : UINT32_MAX;
    if (slot)
        data.slot_param = slot->slotParam;
    data.files = files;
    data.file_count = fileNum;
    if (mountPoint)
        data.mount_point = *mountPoint;
    const auto result = CALL_EXPORT(sceAppMgrSaveDataDataSave, &data);
    if (requiredSizeKiB)
        *requiredSizeKiB = data.required_size_kib;
    if (result == static_cast<int>(SCE_APPMGR_ERROR_SAVEDATA_INVALID_PARAMETER))
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    return result != 0 ? RET_ERROR(result) : 0;
}

EXPORT(int, sceAppUtilSaveDataGetQuota, SceSize *quotaSizeKiB, SceSize *usedSizeKiB, const SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataGetQuota, quotaSizeKiB, usedSizeKiB, mountPoint);

    if (!quotaSizeKiB && !usedSizeKiB)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    SceAppMgrSaveDataQuota data{};
    data.size = sizeof(data);
    if (mountPoint)
        data.mount_point = *mountPoint;
    const auto result = CALL_EXPORT(sceAppMgrSaveDataGetQuota, &data);
    if (result == 0) {
        if (quotaSizeKiB)
            *quotaSizeKiB = data.quota_size_kib;
        if (usedSizeKiB)
            *usedSizeKiB = data.used_size_kib;
        if (quotaSizeKiB && usedSizeKiB && *quotaSizeKiB > 0 && *usedSizeKiB > *quotaSizeKiB)
            *usedSizeKiB = *quotaSizeKiB;
    }
    return result != 0 ? RET_ERROR(result) : 0;
}

EXPORT(int, sceAppUtilSaveDataMount) {
    TRACY_FUNC(sceAppUtilSaveDataMount);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppUtilSaveDataSlotCreate, unsigned int slotId, SceAppUtilSaveDataSlotParam *param, SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataSlotCreate, slotId, param, mountPoint);
    if (!param || slotId >= SCE_APPUTIL_SAVEDATA_SLOT_MAX)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    SceAppMgrSaveDataSlot data{};
    data.size = sizeof(data);
    data.slot_id = slotId;
    data.param = *param;
    if (mountPoint)
        data.mount_point = *mountPoint;
    const auto result = CALL_EXPORT(sceAppMgrSaveDataSlotCreate, &data);
    if (result == static_cast<int>(SCE_APPMGR_ERROR_SAVEDATA_SLOT_EXISTS))
        return RET_ERROR(SCE_APPUTIL_ERROR_SAVEDATA_SLOT_EXISTS);
    return result != 0 ? RET_ERROR(result) : 0;
}

EXPORT(int, sceAppUtilSaveDataSlotDelete, unsigned int slotId, SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataSlotDelete, slotId, mountPoint);
    if (slotId >= SCE_APPUTIL_SAVEDATA_SLOT_MAX)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    SceAppMgrSaveDataSlotDelete data{};
    data.size = sizeof(data);
    data.slot_id = slotId;
    if (mountPoint)
        data.mount_point = *mountPoint;
    const auto result = CALL_EXPORT(sceAppMgrSaveDataSlotDelete, &data);
    if (result == static_cast<int>(SCE_APPMGR_ERROR_SAVEDATA_SLOT_NOT_FOUND))
        return RET_ERROR(SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND);
    return result != 0 ? RET_ERROR(result) : 0;
}

EXPORT(int, sceAppUtilSaveDataSlotGetParam, unsigned int slotId, SceAppUtilSaveDataSlotParam *param, SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataSlotGetParam, slotId, param, mountPoint);
    if (!param || slotId >= SCE_APPUTIL_SAVEDATA_SLOT_MAX)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    SceAppMgrSaveDataSlot data{};
    data.size = sizeof(data);
    data.slot_id = slotId;
    if (mountPoint)
        data.mount_point = *mountPoint;
    const auto result = CALL_EXPORT(sceAppMgrSaveDataSlotGetParam, &data);
    if (result == static_cast<int>(SCE_APPMGR_ERROR_SAVEDATA_SLOT_NOT_FOUND))
        return RET_ERROR(SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND);
    if (result == 0) {
        *param = data.param;
        param->status = 0;
    }
    return result != 0 ? RET_ERROR(result) : 0;
}

EXPORT(SceInt32, sceAppUtilSaveDataSlotSearch, SceAppUtilWorkBuffer *workBuf, const SceAppUtilSaveDataSlotSearchCond *cond,
    SceAppUtilSlotSearchResult *result, const SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataSlotSearch, workBuf, cond, result, mountPoint);
    STUBBED("No sort slot list");

    if (!cond || !result || cond->from > SCE_APPUTIL_SAVEDATA_SLOT_MAX
        || cond->range > SCE_APPUTIL_SAVEDATA_SLOT_MAX - cond->from
        || (cond->type != SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EXIST_SLOT && cond->type != SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EMPTY_SLOT)
        || (workBuf && workBuf->bufSize < cond->range * sizeof(SceAppUtilSaveDataSlot)))
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    if (workBuf)
        result->slotList = Ptr<SceAppUtilSaveDataSlot>(workBuf->buf.address());

    result->hitNum = 0;
    auto slotList = result->slotList.get(emuenv.mem);
    const std::lock_guard lock(sdslot_mutex);
    for (auto i = cond->from; i < (cond->from + cond->range); i++) {
        if (slotList) {
            slotList[i - cond->from].id = -1;
            slotList[i - cond->from].status = 0;
            slotList[i - cond->from].userParam = 0;
            slotList[i - cond->from].emptyParam = Ptr<SceAppUtilSaveDataSlotEmptyParam>(0);
        }

        SceAppMgrSaveDataSlot data{};
        data.size = sizeof(data);
        data.slot_id = i;
        if (mountPoint)
            data.mount_point = *mountPoint;
        const auto slot_result = CALL_EXPORT(sceAppMgrSaveDataSlotGetParam, &data);
        if (slot_result != 0 && slot_result != static_cast<int>(SCE_APPMGR_ERROR_SAVEDATA_SLOT_NOT_FOUND))
            return slot_result;
        switch (cond->type) {
        case SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EXIST_SLOT:
            if (slot_result == 0) {
                if (slotList) {
                    slotList[result->hitNum].userParam = data.param.userParam;
                    slotList[result->hitNum].status = data.param.status;
                    slotList[result->hitNum].id = i;
                }
                result->hitNum++;
            }
            break;
        case SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EMPTY_SLOT:
            if (slot_result == static_cast<int>(SCE_APPMGR_ERROR_SAVEDATA_SLOT_NOT_FOUND)) {
                if (slotList)
                    slotList[result->hitNum].id = i;
                result->hitNum++;
            }
            break;
        default: break;
        }
    }

    return 0;
}

EXPORT(SceInt32, sceAppUtilSaveDataSlotSetParam, SceAppUtilSaveDataSlotId slotId, SceAppUtilSaveDataSlotParam *param, SceAppUtilMountPoint *mountPoint) {
    TRACY_FUNC(sceAppUtilSaveDataSlotSetParam, slotId, param, mountPoint);
    if (!param || slotId >= SCE_APPUTIL_SAVEDATA_SLOT_MAX)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    SceAppMgrSaveDataSlot data{};
    data.size = sizeof(data);
    data.slot_id = slotId;
    data.param = *param;
    if (mountPoint)
        data.mount_point = *mountPoint;
    const auto result = CALL_EXPORT(sceAppMgrSaveDataSlotSetParam, &data);
    if (result == static_cast<int>(SCE_APPMGR_ERROR_SAVEDATA_SLOT_NOT_FOUND))
        return RET_ERROR(SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND);
    return result != 0 ? RET_ERROR(result) : 0;
}

EXPORT(int, sceAppUtilSaveDataUmount) {
    TRACY_FUNC(sceAppUtilSaveDataUmount);
    return UNIMPLEMENTED();
}

SceInt32 execute_savedata_safe_memory(EmuEnvState &emuenv, void *buf, SceSize bufSize, SceOff offset, const char *export_name, bool save) {
    const std::lock_guard savedata_lock(emuenv.io.savedata_mutex);
    bool protected_savedata;
    {
        const std::lock_guard lock(emuenv.io.appmeta_mutex);
        protected_savedata = !emuenv.io.savedata_pfs_mount.empty();
    }
    const auto path = construct_savedata0_path("sce_sys/safemem", "dat");
    std::vector<uint8_t> data;
    int open_error = 0;
    // Open and read the existing safe memory file, if present.
    const bool found = read_file_to_buffer(emuenv.io, path.c_str(), emuenv.vita_fs_path, data, export_name,
        SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE, &open_error, true);
    if (!found && open_error != SCE_ERROR_ERRNO_ENOENT)
        return RET_ERROR(SCE_ERROR_ERRNO_EIO);
    // Load can return 0 when the file does not exist.
    if ((!found || data.empty()) && !save)
        return 0;
    data.resize(SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE);
    if (!save) {
        std::memcpy(buf, data.data() + offset, bufSize);
        return bufSize;
    }
    // Create or update safe memory with the supplied buffer.
    std::memcpy(data.data() + offset, buf, bufSize);
    if (protected_savedata) {
        const auto result = write_protected_savedata(emuenv.io, fs::path("sce_sys") / "safemem.dat", 0, data, data.size());
        return result != 0 ? RET_ERROR(result) : static_cast<SceInt32>(bufSize);
    }
    const auto fd = open_file(emuenv.io, path.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, emuenv.vita_fs_path, export_name);
    if (fd < 0)
        return RET_ERROR(fd);
    const auto written = write_file(fd, data.data(), static_cast<SceSize>(data.size()), emuenv.io, export_name);
    const auto closed = close_file(emuenv.io, fd, export_name);
    if (written < 0)
        return RET_ERROR(written);
    if (static_cast<size_t>(written) != data.size())
        return RET_ERROR(SCE_ERROR_ERRNO_EIO);
    if (closed < 0)
        return RET_ERROR(closed);
    return bufSize;
}

EXPORT(SceInt32, sceAppUtilLoadSafeMemory, void *buf, SceSize bufSize, SceOff offset) {
    TRACY_FUNC(sceAppUtilLoadSafeMemory, buf, bufSize, offset);
    if (!buf || offset < 0 || static_cast<uint64_t>(offset) + bufSize > SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    const auto result = CALL_EXPORT(sceAppMgrLoadSafeMemory, buf, bufSize, offset);
    return result < 0 ? RET_ERROR(result) : result;
}

EXPORT(SceInt32, sceAppUtilSaveSafeMemory, const void *buf, SceSize bufSize, SceOff offset) {
    TRACY_FUNC(sceAppUtilSaveSafeMemory, buf, bufSize, offset);
    if (!buf || offset < 0 || static_cast<uint64_t>(offset) + bufSize > SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    const auto result = CALL_EXPORT(sceAppMgrSaveSafeMemory, buf, bufSize, offset);
    return result < 0 ? RET_ERROR(result) : result;
}

EXPORT(int, sceAppUtilShutdown) {
    TRACY_FUNC(sceAppUtilShutdown);
    const auto thread = emuenv.kernel.get_thread(thread_id);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    if (emuenv.kernel.is_primary_address_space(thread->cpu->mem))
        return UNIMPLEMENTED();
    auto *state = find_secondary_app_util_state(emuenv, thread->cpu->mem);
    if (!state) {
        LOG_ERROR("sceAppUtilShutdown cannot find the secondary process");
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }
    return shutdown_secondary_app_util(*state);
}

EXPORT(int, sceAppUtilStoreBrowse) {
    TRACY_FUNC(sceAppUtilStoreBrowse);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAppUtilSystemParamGetInt, SceSystemParamId paramId, SceInt32 *value) {
    TRACY_FUNC(sceAppUtilSystemParamGetInt, paramId, value);
    if (!value)
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);

    switch (paramId) {
    case SCE_SYSTEM_PARAM_ID_LANG:
        *value = (SceSystemParamLang)emuenv.cfg.sys_lang;
        return 0;
    case SCE_SYSTEM_PARAM_ID_ENTER_BUTTON:
        *value = (SceSystemParamEnterButtonAssign)emuenv.cfg.sys_button;
        return 0;
    case SCE_SYSTEM_PARAM_ID_DATE_FORMAT:
        *value = (SceSystemParamDateFormat)emuenv.cfg.sys_date_format;
        return 0;
    case SCE_SYSTEM_PARAM_ID_TIME_FORMAT:
        *value = (SceSystemParamTimeFormat)emuenv.cfg.sys_time_format;
        return 0;
    case SCE_SYSTEM_PARAM_ID_TIME_ZONE:
    case SCE_SYSTEM_PARAM_ID_SUMMERTIME:
        STUBBED("No support Time Zone and Summer Time, give 0 value");
        *value = 0;
        return 0;
    default:
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    }
}

EXPORT(int, sceAppUtilSystemParamGetString, unsigned int paramId, SceChar8 *buf, SceSize bufSize) {
    TRACY_FUNC(sceAppUtilSystemParamGetString, paramId, buf, bufSize);
    switch (paramId) {
    case SCE_SYSTEM_PARAM_ID_USER_NAME: {
        if (bufSize == 0) {
            return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
        }
        // copy user name into buf with size SCE_SYSTEM_PARAM_USERNAME_MAXSIZE or bufSize, whichever is smaller
        const auto copySize = std::min(bufSize, static_cast<unsigned int>(SCE_SYSTEM_PARAM_USERNAME_MAXSIZE));
        std::strncpy(reinterpret_cast<char *>(buf), emuenv.io.user_name.c_str(), copySize - 1);
        buf[copySize - 1] = '\0';
        // fallback to hostname
        if (buf[0] == '\0') {
            char devname[SCE_SYSTEM_PARAM_USERNAME_MAXSIZE];
            if (gethostname(devname, SCE_SYSTEM_PARAM_USERNAME_MAXSIZE) == 0) {
                std::strncpy(reinterpret_cast<char *>(buf), devname, copySize - 1);
                buf[copySize - 1] = '\0';
            }
            // double fallback to "Vita3K"
            if (buf[0] == '\0') {
                std::strncpy(reinterpret_cast<char *>(buf), "Vita3K", copySize - 1);
                buf[copySize - 1] = '\0';
            }
        }
        break;
    }
    default:
        return RET_ERROR(SCE_APPUTIL_ERROR_PARAMETER);
    }
    return 0;
}
