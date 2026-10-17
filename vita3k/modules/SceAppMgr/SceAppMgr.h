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
#include <mem/functions.h>
#include <module/module.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

struct IOState;
struct ThreadState;

struct ProcessIoContext {
    std::shared_ptr<ThreadState> thread;
    IOState *io;
};

struct SecondaryAppUtilState {
    std::mutex mutex;
    IOState *io = nullptr;
    bool initialized = false;
};

SecondaryAppUtilState *find_secondary_app_util_state(EmuEnvState &emuenv, const MemState &mem);
IOState *find_secondary_process_io(EmuEnvState &emuenv, const MemState &mem);
IOState *find_process_io(EmuEnvState &emuenv, const MemState &mem);
bool is_secondary_process_id(EmuEnvState &emuenv, SceUID pid);
std::expected<ProcessIoContext, SceInt32> get_process_io_context(EmuEnvState &emuenv, SceUID thread_id, const char *operation);
IOState *begin_secondary_sysmodule_load(EmuEnvState &emuenv, const MemState &mem, SceUInt32 module_id);
void end_secondary_sysmodule_load(EmuEnvState &emuenv, const MemState &mem, SceUInt32 module_id);
SceUID open_secondary_process_terminal(EmuEnvState &emuenv, const MemState &mem, const char *path, int flags, const char *export_name);

enum SceAppMgrErrorCode : uint32_t {
    SCE_APPMGR_ERROR_INVALID_PARAMETER = 0x80800001, //!< Invalid parameter (NULL)
    SCE_APPMGR_ERROR_PERMISSION = 0x80800009,
    SCE_APPMGR_ERROR_INVALID_PARAMETER2 = 0x80801000, //!< Invalid parameter (wrong value)
    SCE_APPMGR_ERROR_INVALID_POINTER = 0x80801001, //!< Invalid pointer
    SCE_APPMGR_ERROR_INVALID_APP_PARAM = 0x80801042,
    SCE_APPMGR_ERROR_SAVEDATA_SLOT_EXISTS = 0x8080108a, //!< Save data slot already exists
    SCE_APPMGR_ERROR_SAVEDATA_SLOT_NOT_FOUND = 0x8080108b, //!< Save data slot does not exist
    SCE_APPMGR_ERROR_SAVEDATA_INVALID_PARAMETER = 0x8080108e, //!< Invalid save data parameter
    SCE_APPMGR_ERROR_BUSY = 0x80802000, //!< Busy
    SCE_APPMGR_ERROR_APP_NOT_FOUND = 0x80802012, //!< Application not found
    SCE_APPMGR_ERROR_STATE = 0x80802013, //!< Invalid state
    SCE_APPMGR_ERROR_APP_PARAM_READ = 0x80802015, //!< The app param was already read
    SCE_APPMGR_ERROR_NULL_POINTER = 0x80802016, //!< NULL pointer
    SCE_APPMGR_ERROR_INVALID = 0x8080201A, //!< Invalid param
    SCE_APPMGR_ERROR_SHELL_ONLY = 0x8080201C,
    SCE_APPMGR_ERROR_TOO_LONG_ARGV = 0x8080201D, //!< argv is too long
    SCE_APPMGR_ERROR_INVALID_SELF_PATH = 0x8080201E, //!< Invalid SELF path
    SCE_APPMGR_ERROR_NOT_SHELL = 0x8080201F,
    SCE_APPMGR_ERROR_NO_EVENT = 0x80802020, //!< No application event available
    SCE_APPMGR_ERROR_BGM_PORT_BUSY = 0x80803000 //!< BGM port was occupied and could not be secured
};

struct SceAppMgrEvent {
    SceUInt32 event;
    SceInt32 app_id;
    SceUInt8 param[56];
};

static_assert(sizeof(SceAppMgrEvent) == 64);

struct SceAppMgrShellEvent {
    SceUInt32 event;
    SceUID process_id;
    SceInt32 app_id;
    SceUInt8 reserved[52];
};

static_assert(sizeof(SceAppMgrShellEvent) == 64);

inline bool write_appmgr_shell_event(MemState &mem, Ptr<SceAppMgrShellEvent> destination, const SceAppMgrShellEvent &event) {
    const GuestMemoryRange range{ destination.address(), sizeof(event) };
    const uint64_t pin = pin_guest_memory(mem, std::span(&range, 1));
    if (!pin)
        return false;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&event);
    for (size_t i = 0; i < sizeof(event); ++i)
        *Ptr<uint8_t>(destination.address() + static_cast<Address>(i)).get(mem) = bytes[i];
    unpin_guest_memory(mem, pin);
    return true;
}

int appmgr_receive_shell_event(EmuEnvState &emuenv, SceUID thread_id, Ptr<SceAppMgrShellEvent> event);
int appmgr_get_running_app_id_list(EmuEnvState &emuenv, SceUID thread_id, Ptr<SceInt32> app_ids, SceUInt32 count);
int appmgr_set_back_render_port_owner(EmuEnvState &emuenv, SceUID thread_id, SceInt32 app_id);

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

struct AppMgrLaunchOption {
    SceUInt32 size;
    SceUInt32 flags;
    char app_name[32];
    Ptr<void> status;
    SceUInt32 physical_contiguous_size;
    SceUInt32 infobar;
    SceUInt32 launch_argument_marker;
    char proxy_app_name[32];
    SceUInt32 launch_origin;
    SceUInt32 reserved;
    Ptr<void> param_result;
    SceUInt32 use_common_dialog;
    SceUInt32 enlarge_game_memory_size;
    SceUInt32 trailing[5];
};

static_assert(sizeof(AppMgrLaunchOption) == 128);
static_assert(offsetof(AppMgrLaunchOption, param_result) == 96);

struct AppMgrLaunchStatus {
    SceUID launch_source_process_id;
    SceUInt32 launch_mode;
    SceUInt32 protection_mode;
    char app_name[32];
    SceUInt32 param_delivery_handled;
    SceInt32 app_id;
    SceUID process_id;
    SceUInt32 activated;
    SceUInt32 process_run_requested;
    SceUInt32 network_disconnection_warning;
    SceInt32 launch_source_app_id;
    SceUInt32 infobar;
    SceUInt32 launch_argument_marker;
    char bgm_proxy_app_name[32];
    SceUInt8 launch_origin;
    SceUInt8 budget_type;
    SceUInt8 padding[2];
    SceUInt32 process_attr;
    SceInt32 system_orientation;
    SceUInt32 recommended_orientation_activated;
};

static_assert(sizeof(AppMgrLaunchStatus) == 128);
static_assert(offsetof(AppMgrLaunchStatus, app_id) == 48);
static_assert(offsetof(AppMgrLaunchStatus, launch_origin) == 112);

struct SecondaryAppStatus {
    AppMgrLaunchStatus status;
};

std::optional<SecondaryAppStatus> find_secondary_app_by_id(EmuEnvState &emuenv, SceUID app_id);
std::optional<SecondaryAppStatus> find_secondary_app_by_name(EmuEnvState &emuenv, std::string_view name);

inline AppMgrLaunchStatus make_appmgr_launch_status(std::string_view name, SceUID source_pid,
    SceUID pid, SceInt32 app_id, const AppMgrLaunchOption *option) {
    AppMgrLaunchStatus status{};
    status.launch_source_process_id = source_pid;
    status.launch_source_app_id = 0x1002;
    std::copy_n(name.begin(), std::min(name.size(), sizeof(status.app_name)), status.app_name);
    status.app_id = app_id;
    status.process_id = pid;
    status.budget_type = 1;
    status.launch_argument_marker = option ? option->launch_argument_marker : 1;
    if (option) {
        status.infobar = option->infobar;
        status.launch_origin = option->launch_origin == 1 ? 1 : 0;
    }
    return status;
}

inline bool write_appmgr_launch_status(MemState &mem, Ptr<void> destination, const AppMgrLaunchStatus &status) {
    const GuestMemoryRange range{ destination.address(), sizeof(status) };
    const uint64_t pin = pin_guest_memory(mem, std::span(&range, 1));
    if (!pin)
        return false;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&status);
    for (size_t i = 0; i < sizeof(status); ++i)
        *Ptr<uint8_t>(destination.address() + static_cast<Address>(i)).get(mem) = bytes[i];
    unpin_guest_memory(mem, pin);
    return true;
}

inline std::optional<AppMgrLaunchOption> read_appmgr_launch_option(MemState &mem, Ptr<const void> option) {
    AppMgrLaunchOption result{};
    const GuestMemoryRange range{ option.address(), sizeof(result) };
    const uint64_t pin = pin_guest_memory(mem, std::span(&range, 1));
    if (!pin)
        return std::nullopt;
    auto *bytes = reinterpret_cast<uint8_t *>(&result);
    for (size_t i = 0; i < sizeof(result); ++i)
        bytes[i] = *Ptr<const uint8_t>(option.address() + static_cast<Address>(i)).get(mem);
    unpin_guest_memory(mem, pin);
    return result;
}

inline bool appmgr_launch_option_supported(const AppMgrLaunchOption &option) {
    return !option.flags && !option.param_result && !option.physical_contiguous_size
        && !option.proxy_app_name[0] && !option.use_common_dialog && !option.enlarge_game_memory_size;
}

inline bool appmgr_is_basic_ux0_launch(std::string_view path, std::string_view title_id) {
    return !title_id.empty() && title_id.size() <= 32
        && std::ranges::all_of(title_id, [](unsigned char ch) {
               return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
           })
        && path == "ux0:app/" + std::string(title_id) + "/eboot.bin";
}

inline std::optional<AppMgrLaunchAppByPathArgs> read_appmgr_launch_path_args(const MemState &mem, Ptr<const AppMgrLaunchAppByPathArgs> arguments) {
    if (!arguments || !is_valid_addr_len(mem, arguments.address(), sizeof(AppMgrLaunchAppByPathArgs)))
        return std::nullopt;

    AppMgrLaunchAppByPathArgs args{};
    auto *bytes = reinterpret_cast<uint8_t *>(&args);
    for (size_t i = 0; i < sizeof(args); ++i)
        bytes[i] = *Ptr<const uint8_t>(arguments.address() + static_cast<Address>(i)).get(mem);
    return args;
}

int appmgr_launch_app_by_name_for_shell(EmuEnvState &emuenv, SceUID thread_id, int flags,
    Ptr<const char> name, Ptr<const char> param);
int appmgr_launch_app_by_path(EmuEnvState &emuenv, SceUID thread_id, Ptr<const char> path, Ptr<const char> title_id,
    SceUInt32 budget_size, Ptr<const char> param, SceUInt32 process_attr, Ptr<const void> option);
int appmgr_get_app_param_int(EmuEnvState &emuenv, SceUID thread_id, SceUID pid, int param, Ptr<SceInt32> value);
int appmgr_get_app_param_string(EmuEnvState &emuenv, SceUID thread_id, SceUID pid, int param, Ptr<char> output, SceSize length);

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
DECL_EXPORT(SceInt32, _sceAppMgrAppParamGetString, int pid, int param, Ptr<char> string, Ptr<const sceAppMgrAppParamGetStringOptParam> optParam);
DECL_EXPORT(SceInt32, _sceAppMgrGetAppParam, char *param);
DECL_EXPORT(int, _sceAppMgrReceiveEvent, SceAppMgrEvent *event);
DECL_EXPORT(int, _sceAppMgrReceiveEventNum, SceUInt32 *eventNum);
DECL_EXPORT(SceInt32, _sceAppMgrLoadExec, const char *appPath, Ptr<char> const argv[], const SceAppMgrLoadExecOptParam *optParam);
DECL_EXPORT(SceInt32, _sceAppMgrMmsMount, SceInt32 id, char *mount_point);

SceInt32 __sceAppMgrGetAppState(SceAppMgrAppState *appState, SceUInt32 sizeofSceAppMgrAppState, SceUInt32 buildVersion);
SceInt32 _sceAppMgrLoadExec(const char *appPath, char *const argv[], const SceAppMgrLoadExecOptParam *optParam);
