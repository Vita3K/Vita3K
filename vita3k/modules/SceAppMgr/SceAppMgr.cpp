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

#include "SceAppMgr.h"
#include "app_param.h"
#include "bgm.h"

#include <app/functions.h>
#include <config/config.h>
#include <cpu/state.h>
#include <display/process_frames.h>
#include <io/functions.h>
#include <io/io.h>
#include <io/state.h>
#include <kernel/state.h>
#include <kernel/sync_primitives.h>
#include <kernel/thread/thread_state.h>
#include <mem/state.h>
#include <modules/module_parent.h>
#include <packages/license.h>
#include <packages/sfo.h>
#include <renderer/state.h>
#include <util/string_utils.h>
#include <util/tracy.h>
#include <vfs/mounts.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_set>

TRACY_MODULE_NAME(SceAppMgr);

struct SecondaryProcess {
    enum class State { created,
        starting,
        started };
    MemState mem;
    IOState io{};
    SceUID pid = 0;
    SceUID app_id = 0;
    SceUID main_thread_id = 0;
    SceUID main_module_id = 0;
    State state = State::created;
    bool activated = false;
    AppMgrLaunchStatus launch_status{};
    std::string path;
    std::string title_id;
    std::optional<SfoFile> sfo;
    std::unordered_set<SceUInt32> sysmodules_loading;
    SecondaryAppUtilState apputil;

    ~SecondaryProcess() {
        if (mem.memory)
            deinit_mem(mem);
    }
};

struct AppMgrProcesses {
    std::mutex mutex;
    std::mutex activation_mutex;
    std::vector<std::unique_ptr<SecondaryProcess>> processes;
    std::deque<SceAppMgrShellEvent> shell_events;
    size_t reserved_shell_events = 0;
    SceUInt32 app_generation = 0;
};

SecondaryAppUtilState *find_secondary_app_util_state(EmuEnvState &emuenv, const MemState &mem) {
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes)
        return nullptr;
    const std::lock_guard lock(processes->mutex);
    for (const auto &process : processes->processes) {
        if (&process->mem == &mem)
            return &process->apputil;
    }
    return nullptr;
}

IOState *find_secondary_process_io(EmuEnvState &emuenv, const MemState &mem) {
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes)
        return nullptr;
    const std::lock_guard lock(processes->mutex);
    for (const auto &process : processes->processes) {
        if (&process->mem == &mem)
            return &process->io;
    }
    return nullptr;
}

IOState *find_process_io(EmuEnvState &emuenv, const MemState &mem) {
    return emuenv.kernel.is_primary_address_space(mem) ? &emuenv.io : find_secondary_process_io(emuenv, mem);
}

bool is_secondary_process_id(EmuEnvState &emuenv, SceUID pid) {
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes)
        return false;
    const std::lock_guard lock(processes->mutex);
    return std::ranges::any_of(processes->processes, [pid](const auto &process) {
        return process->app_id && process->pid == pid;
    });
}

std::expected<ProcessIoContext, SceInt32> get_process_io_context(EmuEnvState &emuenv, SceUID thread_id, const char *operation) {
    const auto thread = emuenv.kernel.get_thread(thread_id);
    if (!thread) {
        LOG_ERROR("{}: unknown thread {}", operation, thread_id);
        return std::unexpected(static_cast<SceInt32>(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID));
    }
    IOState *io = find_process_io(emuenv, thread->cpu->mem);
    if (!io) {
        LOG_ERROR("{}: no IO state for thread {}", operation, thread_id);
        return std::unexpected(static_cast<SceInt32>(SCE_KERNEL_ERROR_ERROR));
    }
    return ProcessIoContext{ thread, io };
}

static bool is_shell_caller(EmuEnvState &emuenv, SceUID thread_id) {
    const auto thread = emuenv.kernel.get_thread(thread_id);
    if (!thread)
        return false;
    const auto identity = emuenv.kernel.get_process_identity(thread->cpu->mem);
    return identity && identity->app_path == "vs0:vsh/shell";
}

static bool uses_shell_builtin_app_params(EmuEnvState &emuenv, SceUID thread_id, SceUID pid) {
    const auto primary = emuenv.kernel.get_process_identity(emuenv.mem);
    const auto caller = emuenv.kernel.get_thread_process_id(thread_id);
    return primary && caller && primary->is_shell() && *caller == primary->pid
        && (pid == 0 || pid == primary->pid) && emuenv.sfo_handle.header.magic != 0x46535000;
}

static int get_process_sfo(EmuEnvState &emuenv, SceUID thread_id, SceUID pid, SfoFile &sfo) {
    const auto caller_pid = emuenv.kernel.get_thread_process_id(thread_id);
    if (!caller_pid)
        return static_cast<int>(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    if (!pid)
        pid = *caller_pid;
    if (pid != *caller_pid && !is_shell_caller(emuenv, thread_id))
        return static_cast<int>(SCE_APPMGR_ERROR_PERMISSION);
    const auto primary = emuenv.kernel.get_process_identity(emuenv.mem);
    if (primary && primary->pid == pid) {
        if (emuenv.sfo_handle.header.magic != 0x46535000) {
            LOG_ERROR("No valid application SFO for process {}", pid);
            return static_cast<int>(SCE_APPMGR_ERROR_INVALID);
        }
        sfo = emuenv.sfo_handle;
        return 0;
    }
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes) {
        LOG_ERROR("Cannot resolve application parameters without AppMgr process state");
        return static_cast<int>(SCE_KERNEL_ERROR_ERROR);
    }
    const std::lock_guard lock(processes->mutex);
    for (const auto &process : processes->processes) {
        if (process->pid != pid || !process->app_id)
            continue;
        if (!process->sfo) {
            LOG_ERROR("No valid application SFO for secondary process {}", pid);
            return static_cast<int>(SCE_APPMGR_ERROR_INVALID);
        }
        sfo = *process->sfo;
        return 0;
    }
    return static_cast<int>(SCE_APPMGR_ERROR_APP_NOT_FOUND);
}

static SecondaryAppStatus secondary_status(const SecondaryProcess &process) {
    auto status = process.launch_status;
    status.activated = process.activated;
    status.process_run_requested = process.state == SecondaryProcess::State::started;
    return { status };
}

std::optional<SecondaryAppStatus> find_secondary_app_by_id(EmuEnvState &emuenv, SceUID app_id) {
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes)
        return std::nullopt;
    const std::lock_guard lock(processes->mutex);
    for (const auto &process : processes->processes) {
        if (process->app_id && process->app_id == app_id)
            return secondary_status(*process);
    }
    return std::nullopt;
}

std::optional<SecondaryAppStatus> find_secondary_app_by_name(EmuEnvState &emuenv, std::string_view name) {
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes)
        return std::nullopt;
    const std::lock_guard lock(processes->mutex);
    for (const auto &process : processes->processes) {
        if (process->app_id && process->title_id == name)
            return secondary_status(*process);
    }
    return std::nullopt;
}

int appmgr_get_running_app_id_list(EmuEnvState &emuenv, SceUID thread_id, Ptr<SceInt32> app_ids, SceUInt32 count) {
    if (!is_shell_caller(emuenv, thread_id))
        return static_cast<int>(SCE_APPMGR_ERROR_SHELL_ONLY);
    if (!app_ids)
        return static_cast<int>(SCE_APPMGR_ERROR_NULL_POINTER);
    if (count == 0 || count > 20) {
        LOG_ERROR("Invalid running application list capacity {}", count);
        return static_cast<int>(SCE_APPMGR_ERROR_INVALID_PARAMETER2);
    }
    const auto thread = emuenv.kernel.get_thread(thread_id);
    if (!thread)
        return static_cast<int>(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes) {
        LOG_ERROR("Cannot list running applications without AppMgr process state");
        return static_cast<int>(SCE_KERNEL_ERROR_ERROR);
    }
    std::array<SceInt32, 20> ids{};
    size_t found = 0;
    {
        const std::lock_guard lock(processes->mutex);
        for (const auto &process : processes->processes) {
            if (process->app_id && process->state == SecondaryProcess::State::started) {
                ids[found++] = process->app_id;
                if (found == count)
                    break;
            }
        }
    }
    auto &mem = thread->cpu->mem;
    const GuestMemoryRange range{ app_ids.address(), static_cast<uint32_t>(count * sizeof(SceInt32)) };
    const uint64_t pin = pin_guest_memory(mem, std::span(&range, 1));
    if (!pin) {
        LOG_ERROR("Invalid running application list destination {}", log_hex(app_ids.address()));
        return static_cast<int>(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    }
    const auto *bytes = reinterpret_cast<const uint8_t *>(ids.data());
    for (size_t i = 0; i < range.size; ++i)
        *Ptr<uint8_t>(app_ids.address() + static_cast<Address>(i)).get(mem) = bytes[i];
    unpin_guest_memory(mem, pin);
    return static_cast<int>(found);
}

int appmgr_receive_shell_event(EmuEnvState &emuenv, SceUID thread_id, Ptr<SceAppMgrShellEvent> event) {
    if (!is_shell_caller(emuenv, thread_id))
        return static_cast<int>(SCE_APPMGR_ERROR_NOT_SHELL);
    if (!event)
        return static_cast<int>(SCE_APPMGR_ERROR_NULL_POINTER);
    const auto thread = emuenv.kernel.get_thread(thread_id);
    if (!thread)
        return static_cast<int>(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    auto &mem = thread->cpu->mem;
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes) {
        LOG_ERROR("Cannot receive a Shell event without AppMgr process state");
        return static_cast<int>(SCE_KERNEL_ERROR_ERROR);
    }
    int result = static_cast<int>(SCE_APPMGR_ERROR_APP_PARAM_READ);
    {
        const std::lock_guard lock(processes->mutex);
        if (!processes->shell_events.empty()) {
            if (write_appmgr_shell_event(mem, event, processes->shell_events.front())) {
                LOG_DEBUG("Shell received AppMgr event {} for process {} and app {}",
                    processes->shell_events.front().event, processes->shell_events.front().process_id,
                    log_hex(processes->shell_events.front().app_id));
                processes->shell_events.pop_front();
                result = 0;
            } else {
                LOG_ERROR("Invalid Shell event destination {}", log_hex(event.address()));
                result = static_cast<int>(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
            }
        }
    }
    return result;
}

IOState *begin_secondary_sysmodule_load(EmuEnvState &emuenv, const MemState &mem, SceUInt32 module_id) {
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes) {
        LOG_ERROR("Cannot load a secondary system module without AppMgr process state");
        return nullptr;
    }
    const std::lock_guard lock(processes->mutex);
    for (const auto &process : processes->processes) {
        if (&process->mem != &mem)
            continue;
        if (!process->sysmodules_loading.insert(module_id).second) {
            LOG_ERROR("System module {} is already being loaded in process {}", log_hex(module_id), process->pid);
            return nullptr;
        }
        return &process->io;
    }
    LOG_ERROR("Cannot find the AppMgr process for system module {}", log_hex(module_id));
    return nullptr;
}

void end_secondary_sysmodule_load(EmuEnvState &emuenv, const MemState &mem, SceUInt32 module_id) {
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes) {
        LOG_ERROR("Cannot release a secondary system module reservation without AppMgr process state");
        return;
    }
    const std::lock_guard lock(processes->mutex);
    for (const auto &process : processes->processes) {
        if (&process->mem == &mem) {
            if (process->sysmodules_loading.erase(module_id) != 1)
                LOG_ERROR("System module {} had no load reservation in process {}", log_hex(module_id), process->pid);
            return;
        }
    }
    LOG_ERROR("Cannot release system module {}: secondary process not found", log_hex(module_id));
}

SceUID open_secondary_process_terminal(EmuEnvState &emuenv, const MemState &mem, const char *path, int flags, const char *export_name) {
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes) {
        LOG_ERROR("Cannot open secondary process terminal without AppMgr process state");
        return SCE_KERNEL_ERROR_ERROR;
    }
    const std::lock_guard lock(processes->mutex);
    for (const auto &process : processes->processes) {
        if (&process->mem == &mem)
            return open_file(process->io, path, flags, emuenv.vita_fs_path, export_name);
    }
    LOG_ERROR("Cannot open terminal: secondary process not found");
    return SCE_KERNEL_ERROR_ERROR;
}

LIBRARY_INIT(SceAppMgr) {
    emuenv.kernel.obj_store.create<AppMgrBgm>();
    emuenv.kernel.obj_store.create<AppMgrEvents>();
    emuenv.kernel.obj_store.create<AppMgrProcesses>();
}

AppMgrBgmState get_appmgr_bgm_state(EmuEnvState &emuenv) {
    auto *bgm = emuenv.kernel.obj_store.get<AppMgrBgm>();
    return bgm->state();
}

static std::string appmgr_mount_log_string(const MemState &mem, Ptr<const char> input, size_t limit = 256) {
    if (!input)
        return "<null>";

    std::string result;
    for (size_t i = 0; i < limit; ++i) {
        if (i > std::numeric_limits<Address>::max() - input.address())
            return result + "<invalid>";
        const Ptr<const char> current(input.address() + static_cast<Address>(i));
        if (!current.valid(mem))
            return result + "<invalid>";
        const unsigned char ch = *current.get(mem);
        if (ch == 0)
            return result;
        result += ch >= 32 && ch < 127 ? static_cast<char>(ch) : '?';
    }
    return result + "<truncated>";
}

static bool read_mount_string(const MemState &mem, Ptr<const char> input, size_t max_length, std::string &value) {
    value.clear();
    if (!input)
        return false;
    for (size_t i = 0; i <= max_length; ++i) {
        if (i > std::numeric_limits<Address>::max() - input.address())
            return false;
        const Ptr<const char> current(input.address() + static_cast<Address>(i));
        if (!current.valid(mem))
            return false;
        const char ch = *current.get(mem);
        if (ch == '\0')
            return true;
        if (i == max_length)
            return false;
        value.push_back(ch);
    }
    return false;
}

static bool valid_mount_output(const MemState &mem, Ptr<char> output) {
    if (!output || output.address() > std::numeric_limits<Address>::max() - 15)
        return false;
    for (Address i = 0; i < 16; ++i) {
        if (!Ptr<char>(output.address() + i).valid(mem))
            return false;
    }
    return true;
}

static bool valid_mount_sfo(EmuEnvState &emuenv, const std::string &mount_point, const char *relative_sfo, const std::string &title_id, const char *export_name) {
    const std::string sfo_path = mount_point + relative_sfo;
    SceIoStat stat{};
    if (stat_file(emuenv.io, sfo_path.c_str(), &stat, emuenv.vita_fs_path, export_name) < 0
        || !(stat.st_mode & SCE_S_IFREG) || stat.st_size <= 0 || stat.st_size > 32768)
        return false;

    const SceUID fd = open_file(emuenv.io, sfo_path.c_str(), SCE_O_RDONLY, emuenv.vita_fs_path, export_name);
    if (fd < 0)
        return false;
    vfs::FileBuffer buffer(static_cast<size_t>(stat.st_size));
    const int bytes_read = read_file(buffer.data(), emuenv.io, fd, static_cast<SceSize>(buffer.size()), export_name);
    close_file(emuenv.io, fd, export_name);
    sfo::SfoAppInfo info;
    return bytes_read == static_cast<int>(buffer.size())
        && sfo::get_param_info(info, buffer, emuenv.cfg.sys_lang) && info.app_title_id == title_id;
}

EXPORT(SceInt32, __sceAppMgrGetAppState, SceAppMgrAppState *appState, SceUInt32 sizeofSceAppMgrAppState, SceUInt32 buildVersion) {
    TRACY_FUNC(__sceAppMgrGetAppState, appState, sizeofSceAppMgrAppState, buildVersion);
    memset(appState, 0, sizeofSceAppMgrAppState);

    return STUBBED("Set to 0.");
}

EXPORT(int, _sceAppMgrAcidDirSet) {
    TRACY_FUNC(_sceAppMgrAcidDirSet);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAcquireSoundOutExclusive3) {
    TRACY_FUNC(_sceAppMgrAcquireSoundOutExclusive3);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAddContAddMount) {
    TRACY_FUNC(_sceAppMgrAddContAddMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAddContMount) {
    TRACY_FUNC(_sceAppMgrAddContMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAppDataMount) {
    TRACY_FUNC(_sceAppMgrAppDataMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAppDataMountById, int mountId, Ptr<const char> id, Ptr<char> mountPoint) {
    TRACY_FUNC(_sceAppMgrAppDataMountById, mountId, id.address(), mountPoint.address());
    if (mountId != 0x6e) {
        LOG_WARN("Unsupported AppDataMountById type {}", log_hex(mountId));
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    }
    std::string title_id;
    if (!read_mount_string(emuenv.mem, id, 32, title_id) || title_id.empty() || !valid_mount_output(emuenv.mem, mountPoint))
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    if (title_id == "new") {
        LOG_WARN("Writable appmeta promotion is not implemented");
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    }

    std::string mount_point;
    if (!pfsmgr::mount_readonly_appmeta(emuenv.io, emuenv.vita_fs_path, title_id, mount_point)) {
        LOG_WARN("Cannot mount appmeta for '{}'", title_id);
        return RET_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }
    if (mount_point.size() >= 16 || !valid_mount_sfo(emuenv, mount_point, "/param.sfo", title_id, export_name)) {
        LOG_WARN("Invalid or encrypted appmeta SFO for '{}'", title_id);
        if (!pfsmgr::unmount_appmeta(emuenv.io, mount_point))
            LOG_ERROR("Failed to release rejected appmeta mount '{}'", mount_point);
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    for (Address i = 0; i < 16; ++i)
        *Ptr<char>(mountPoint.address() + i).get(emuenv.mem) = i < mount_point.size() ? mount_point[i] : '\0';
    LOG_INFO("Mounted appmeta with readable SFO for '{}' at '{}'", title_id, mount_point);
    return 0;
}

EXPORT(int, _sceAppMgrAppMount) {
    TRACY_FUNC(_sceAppMgrAppMount);
    return UNIMPLEMENTED();
}

static int write_app_param(MemState &mem, Address address, std::span<const uint8_t> bytes) {
    const GuestMemoryRange range{ address, static_cast<uint32_t>(bytes.size()) };
    const uint64_t pin = pin_guest_memory(mem, std::span(&range, 1));
    if (!pin) {
        LOG_ERROR("Invalid application parameter destination {}", log_hex(address));
        return static_cast<int>(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    }
    for (size_t i = 0; i < bytes.size(); ++i)
        *Ptr<uint8_t>(address + static_cast<Address>(i)).get(mem) = bytes[i];
    unpin_guest_memory(mem, pin);
    return 0;
}

int appmgr_get_app_param_int(EmuEnvState &emuenv, SceUID thread_id, SceUID pid, int param, Ptr<SceInt32> value) {
    if (!value)
        return static_cast<int>(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    if (param != 15 && param != 17) {
        LOG_ERROR("Application integer parameter {} is not implemented", param);
        return static_cast<int>(SCE_APPMGR_ERROR_INVALID_APP_PARAM);
    }
    if (!is_shell_caller(emuenv, thread_id))
        return static_cast<int>(SCE_APPMGR_ERROR_PERMISSION);
    if (uses_shell_builtin_app_params(emuenv, thread_id, pid)) {
        const auto thread = emuenv.kernel.get_thread(thread_id);
        if (!thread)
            return static_cast<int>(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
        const std::array<uint8_t, 4> bytes{};
        LOG_DEBUG("AppParamGetInt pid: {}, parameter: {}, Shell value: 0", pid, param);
        return write_app_param(thread->cpu->mem, value.address(), bytes);
    }
    SfoFile sfo{};
    const int result = get_process_sfo(emuenv, thread_id, pid, sfo);
    if (result < 0)
        return result;
    const auto attribute = appmgr_sfo_attribute(sfo, param);
    if (!attribute) {
        LOG_ERROR("Invalid application attribute {} for process {}", param, pid);
        return static_cast<int>(SCE_APPMGR_ERROR_INVALID);
    }
    const auto thread = emuenv.kernel.get_thread(thread_id);
    if (!thread)
        return static_cast<int>(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    const std::array<uint8_t, 4> bytes{ static_cast<uint8_t>(*attribute), static_cast<uint8_t>(*attribute >> 8),
        static_cast<uint8_t>(*attribute >> 16), static_cast<uint8_t>(*attribute >> 24) };
    LOG_DEBUG("AppParamGetInt pid: {}, parameter: {}, value: {}", pid, param, log_hex(*attribute));
    return write_app_param(thread->cpu->mem, value.address(), bytes);
}

int appmgr_get_app_param_string(EmuEnvState &emuenv, SceUID thread_id, SceUID pid, int param, Ptr<char> output, SceSize length) {
    LOG_DEBUG("AppParamGetString request pid: {}, parameter: {}, length: {}", pid, param, length);
    if (!output)
        return static_cast<int>(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    if (param == 100 || param == 101) {
        LOG_WARN("Application parameter cache is not implemented; using SFO parameter {}", param);
        param = param == 100 ? 6 : 9;
    }
    if (param < 6 || param > 14)
        return static_cast<int>(SCE_APPMGR_ERROR_INVALID_PARAMETER2);
    if (uses_shell_builtin_app_params(emuenv, thread_id, pid)) {
        const auto shell_param = appmgr_shell_string_param(param);
        if (!shell_param)
            return static_cast<int>(SCE_APPMGR_ERROR_INVALID_APP_PARAM);
        if (length < shell_param->minimum_size)
            return static_cast<int>(SCE_APPMGR_ERROR_INVALID_PARAMETER2);
        const auto thread = emuenv.kernel.get_thread(thread_id);
        if (!thread)
            return static_cast<int>(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
        length = std::min<SceSize>(length, 1024);
        std::vector<uint8_t> bytes(length, 0);
        std::copy(shell_param->value.begin(), shell_param->value.end(), bytes.begin());
        LOG_DEBUG("AppParamGetString pid: {}, parameter: {}, Shell value: {}", pid, param, shell_param->value);
        return write_app_param(thread->cpu->mem, output.address(), bytes);
    }
    SfoFile sfo{};
    const int result = get_process_sfo(emuenv, thread_id, pid, sfo);
    if (result < 0)
        return result;
    std::string value;
    if (param == 8) {
        if (length < 4)
            return static_cast<int>(SCE_APPMGR_ERROR_INVALID_PARAMETER2);
        const auto category = appmgr_sfo_category(sfo);
        if (!category) {
            LOG_ERROR("Invalid application category for process {}", pid);
            return static_cast<int>(SCE_APPMGR_ERROR_INVALID);
        }
        value = *category;
    } else if (!sfo::get_data_by_id(value, sfo, param)) {
        LOG_ERROR("Application string parameter {} is unavailable for process {}", param, pid);
        return static_cast<int>(SCE_APPMGR_ERROR_INVALID);
    }
    length = std::min<SceSize>(length, 1024);
    if (value.size() >= length)
        return static_cast<int>(SCE_APPMGR_ERROR_INVALID_PARAMETER2);
    std::vector<uint8_t> bytes(length, 0);
    std::copy(value.begin(), value.end(), bytes.begin());
    const auto thread = emuenv.kernel.get_thread(thread_id);
    if (!thread)
        return static_cast<int>(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    LOG_DEBUG("AppParamGetString pid: {}, parameter: {}, value: {}", pid, param, value);
    return write_app_param(thread->cpu->mem, output.address(), bytes);
}

EXPORT(int, _sceAppMgrAppParamGetInt, int pid, int param, Ptr<SceInt32> value) {
    TRACY_FUNC(_sceAppMgrAppParamGetInt, pid, param, value);
    return appmgr_get_app_param_int(emuenv, thread_id, pid, param, value);
}

EXPORT(SceInt32, _sceAppMgrAppParamGetString, int pid, int param, Ptr<char> string, Ptr<const sceAppMgrAppParamGetStringOptParam> optParam) {
    TRACY_FUNC(_sceAppMgrAppParamGetString, pid, param, string, optParam);
    const auto thread = emuenv.kernel.get_thread(thread_id);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    auto &mem = thread->cpu->mem;
    const GuestMemoryRange range{ optParam.address(), sizeof(sceAppMgrAppParamGetStringOptParam) };
    const uint64_t pin = pin_guest_memory(mem, std::span(&range, 1));
    if (!optParam || !pin) {
        if (pin)
            unpin_guest_memory(mem, pin);
        LOG_ERROR("Invalid application string parameter options {}", log_hex(optParam.address()));
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    }
    SceSize length = 0;
    for (size_t i = 0; i < sizeof(length); ++i)
        length |= static_cast<SceSize>(*Ptr<const uint8_t>(optParam.address() + static_cast<Address>(i)).get(mem)) << (i * 8);
    unpin_guest_memory(mem, pin);
    return appmgr_get_app_param_string(emuenv, thread_id, pid, param, string, length);
}

EXPORT(int, _sceAppMgrAppParamSetString) {
    TRACY_FUNC(_sceAppMgrAppParamSetString);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrAppUmount) {
    TRACY_FUNC(_sceAppMgrAppUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrBgdlGetQueueStatus) {
    TRACY_FUNC(_sceAppMgrBgdlGetQueueStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrCaptureFrameBufDMACByAppId) {
    TRACY_FUNC(_sceAppMgrCaptureFrameBufDMACByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrCaptureFrameBufIFTUByAppId) {
    TRACY_FUNC(_sceAppMgrCaptureFrameBufIFTUByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrCheckRifGD) {
    TRACY_FUNC(_sceAppMgrCheckRifGD);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrContentInstallPeriodStart) {
    TRACY_FUNC(_sceAppMgrContentInstallPeriodStart);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrContentInstallPeriodStop) {
    TRACY_FUNC(_sceAppMgrContentInstallPeriodStop);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrConvertVs0UserDrivePath) {
    TRACY_FUNC(_sceAppMgrConvertVs0UserDrivePath);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrDeclareShellProcess2) {
    TRACY_FUNC(_sceAppMgrDeclareShellProcess2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrDestroyAppByName) {
    TRACY_FUNC(_sceAppMgrDestroyAppByName);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrDrmClose, const SceAppMgrDrmOpenParam *param) {
    TRACY_FUNC(_sceAppMgrDrmClose, param);
    if (!param || param->size != sizeof(*param)
        || !std::memchr(param->dir_name, '\0', sizeof(param->dir_name))
        || !std::memchr(param->mount_point, '\0', sizeof(param->mount_point)))
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    return pfsmgr::unmount_addcont(emuenv.io, param->mount_point, param->dir_name);
}

EXPORT(int, _sceAppMgrDrmOpen, const SceAppMgrDrmOpenParam *param) {
    TRACY_FUNC(_sceAppMgrDrmOpen, param);
    if (!param || param->size != sizeof(*param)
        || !std::memchr(param->dir_name, '\0', sizeof(param->dir_name))
        || !std::memchr(param->mount_point, '\0', sizeof(param->mount_point)))
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    return pfsmgr::mount_addcont(emuenv.io, emuenv.vita_fs_path, param->mount_point, param->dir_name);
}

EXPORT(int, _sceAppMgrForceUmount) {
    TRACY_FUNC(_sceAppMgrForceUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGameDataMount, Ptr<const char> source, Ptr<const char> patch, Ptr<const void> license, Ptr<char> mountPoint) {
    TRACY_FUNC(_sceAppMgrGameDataMount, source.address(), patch.address(), license.address(), mountPoint.address());
    std::string source_path;
    std::string patch_path;
    if (!read_mount_string(emuenv.mem, source, 0x123, source_path) || !valid_mount_output(emuenv.mem, mountPoint)
        || (patch && !read_mount_string(emuenv.mem, patch, 0x123, patch_path)))
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    if (license) {
        LOG_WARN("GameDataMount with an explicit license is not implemented");
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    }
    if (source_path.starts_with("ux0:/"))
        source_path.erase(4, 1);
    if (patch_path.starts_with("ux0:/"))
        patch_path.erase(4, 1);
    if (!source_path.starts_with("ux0:app/") || source_path.size() <= 8) {
        LOG_WARN("Unsupported GameDataMount source '{}'", source_path);
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    }
    const auto title_id = source_path.substr(8);
    if (patch && patch_path != "ux0:patch/" + title_id) {
        LOG_WARN("Unsupported GameDataMount patch '{}'", patch_path);
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    }
    std::string mount_point;
    if (!pfsmgr::mount_readonly_game_data(emuenv.io, emuenv.vita_fs_path, title_id, bool(patch), mount_point)) {
        LOG_WARN("Cannot mount game data for '{}'", title_id);
        return RET_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }
    if (!valid_mount_sfo(emuenv, mount_point, "/sce_sys/param.sfo", title_id, export_name)) {
        LOG_WARN("Invalid or encrypted game data SFO for '{}'", title_id);
        if (!pfsmgr::unmount_game_data(emuenv.io, mount_point))
            LOG_ERROR("Failed to release rejected game data mount '{}'", mount_point);
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    for (Address i = 0; i < 16; ++i)
        *Ptr<char>(mountPoint.address() + i).get(emuenv.mem) = i < mount_point.size() ? mount_point[i] : '\0';
    LOG_INFO("Mounted game data for '{}' at '{}' in read-only mode", title_id, mount_point);
    return 0;
}

EXPORT(int, _sceAppMgrGetAppInfo) {
    TRACY_FUNC(_sceAppMgrGetAppInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetAppMgrState) {
    TRACY_FUNC(_sceAppMgrGetAppMgrState);
    return UNIMPLEMENTED();
}

// The size sceAppMgrGetAppParam writes at most, without a terminator if the URI fills it
constexpr size_t APP_PARAM_SIZE = 1024;

EXPORT(SceInt32, _sceAppMgrGetAppParam, char *param) {
    TRACY_FUNC(_sceAppMgrGetAppParam, param);
    if (!param)
        return RET_ERROR(SCE_APPMGR_ERROR_NULL_POINTER);

    // Only the first call gets the launch URI, the ones after it get an empty string
    const std::optional<std::string> uri = emuenv.take_launch_uri();
    if (!uri) {
        param[0] = '\0';
        return SCE_APPMGR_ERROR_APP_PARAM_READ;
    }
    memcpy(param, uri->c_str(), std::min(uri->size() + 1, APP_PARAM_SIZE));
    return 0;
}

EXPORT(int, _sceAppMgrGetAppParam2) {
    TRACY_FUNC(_sceAppMgrGetAppParam2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetBootParam) {
    TRACY_FUNC(_sceAppMgrGetBootParam);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetBudgetInfo) {
    TRACY_FUNC(_sceAppMgrGetBudgetInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetCoredumpStateForShell) {
    TRACY_FUNC(_sceAppMgrGetCoredumpStateForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetCurrentBgmState) {
    TRACY_FUNC(_sceAppMgrGetCurrentBgmState);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetCurrentBgmState2, AppMgrBgmState *state) {
    TRACY_FUNC(_sceAppMgrGetCurrentBgmState2);
    return CALL_EXPORT(sceAppMgrGetCurrentBgmState2, state);
}

EXPORT(int, _sceAppMgrGetDevInfo) {
    TRACY_FUNC(_sceAppMgrGetDevInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetFgAppInfo) {
    TRACY_FUNC(_sceAppMgrGetFgAppInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetIdByName) {
    TRACY_FUNC(_sceAppMgrGetIdByName);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetMediaTypeFromDrive) {
    TRACY_FUNC(_sceAppMgrGetMediaTypeFromDrive);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetMediaTypeFromDriveByPid) {
    TRACY_FUNC(_sceAppMgrGetMediaTypeFromDriveByPid);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetMountProcessNum) {
    TRACY_FUNC(_sceAppMgrGetMountProcessNum);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetNameById) {
    TRACY_FUNC(_sceAppMgrGetNameById);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetPfsDrive) {
    TRACY_FUNC(_sceAppMgrGetPfsDrive);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetPidListForShell) {
    TRACY_FUNC(_sceAppMgrGetPidListForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetRawPath) {
    TRACY_FUNC(_sceAppMgrGetRawPath);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetRawPathOfApp0ByAppIdForShell) {
    TRACY_FUNC(_sceAppMgrGetRawPathOfApp0ByAppIdForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetRawPathOfApp0ByPidForShell) {
    TRACY_FUNC(_sceAppMgrGetRawPathOfApp0ByPidForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetRecommendedScreenOrientation) {
    TRACY_FUNC(_sceAppMgrGetRecommendedScreenOrientation);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetRunningAppIdListForShell, Ptr<SceInt32> app_ids, SceUInt32 count, Ptr<const SceUInt32> arguments) {
    TRACY_FUNC(_sceAppMgrGetRunningAppIdListForShell, app_ids, count, arguments);
    const auto thread = emuenv.kernel.get_thread(thread_id);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    auto &mem = thread->cpu->mem;
    const GuestMemoryRange range{ arguments.address(), 2 * sizeof(SceUInt32) };
    const uint64_t pin = arguments ? pin_guest_memory(mem, std::span(&range, 1)) : 0;
    if (!pin) {
        LOG_ERROR("Invalid running application list syscall arguments {}", log_hex(arguments.address()));
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    }
    SceUInt32 size = 0;
    for (size_t i = 0; i < sizeof(size); ++i)
        size |= static_cast<SceUInt32>(*Ptr<const uint8_t>(arguments.address() + static_cast<Address>(i)).get(mem)) << (i * 8);
    unpin_guest_memory(mem, pin);
    if (count > 20 || size != count * sizeof(SceInt32)) {
        LOG_ERROR("Invalid running application list syscall size {} for capacity {}", size, count);
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER2);
    }
    return appmgr_get_running_app_id_list(emuenv, thread_id, app_ids, count);
}

EXPORT(int, _sceAppMgrGetSaveDataInfo) {
    TRACY_FUNC(_sceAppMgrGetSaveDataInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetSaveDataInfoForSpecialExport) {
    TRACY_FUNC(_sceAppMgrGetSaveDataInfoForSpecialExport);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetStatusByAppId) {
    TRACY_FUNC(_sceAppMgrGetStatusByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetStatusById) {
    TRACY_FUNC(_sceAppMgrGetStatusById);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetStatusByName) {
    TRACY_FUNC(_sceAppMgrGetStatusByName);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetSystemDataFilePlayReady) {
    TRACY_FUNC(_sceAppMgrGetSystemDataFilePlayReady);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetUserDirPath) {
    TRACY_FUNC(_sceAppMgrGetUserDirPath);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetUserDirPathById) {
    TRACY_FUNC(_sceAppMgrGetUserDirPathById);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetVs0UserDataDrive) {
    TRACY_FUNC(_sceAppMgrGetVs0UserDataDrive);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrGetVs0UserModuleDrive) {
    TRACY_FUNC(_sceAppMgrGetVs0UserModuleDrive);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrInitSafeMemoryById) {
    TRACY_FUNC(_sceAppMgrInitSafeMemoryById);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrInstallDirMount) {
    TRACY_FUNC(_sceAppMgrInstallDirMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrIsCameraActive) {
    TRACY_FUNC(_sceAppMgrIsCameraActive);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchAppByName) {
    TRACY_FUNC(_sceAppMgrLaunchAppByName);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchAppByName2) {
    TRACY_FUNC(_sceAppMgrLaunchAppByName2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchAppByName2ForShell) {
    TRACY_FUNC(_sceAppMgrLaunchAppByName2ForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchAppByName2ndStage) {
    TRACY_FUNC(_sceAppMgrLaunchAppByName2ndStage);
    return UNIMPLEMENTED();
}

int appmgr_launch_app_by_name_for_shell(EmuEnvState &emuenv, SceUID thread_id, int flags,
    Ptr<const char> name, Ptr<const char> param) {
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    if (!thread) {
        LOG_ERROR("LaunchAppByNameForShell: unknown thread {}", thread_id);
        return static_cast<int>(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }

    LOG_INFO("LaunchAppByNameForShell flags: {}, name: {}, param: {}",
        log_hex(flags), appmgr_mount_log_string(thread->cpu->mem, name, 32), log_hex(param.address()));
    LOG_ERROR("LaunchAppByNameForShell requires multi-process support; application was not launched");
    return static_cast<int>(SCE_KERNEL_ERROR_NOT_IMPLEMENTED);
}

EXPORT(int, _sceAppMgrLaunchAppByNameForShell, int flags, Ptr<const char> name,
    Ptr<const char> param, Ptr<void> arguments) {
    TRACY_FUNC(_sceAppMgrLaunchAppByNameForShell, flags, name, param, arguments);
    return appmgr_launch_app_by_name_for_shell(emuenv, thread_id, flags, name, param);
}

int appmgr_launch_app_by_path(EmuEnvState &emuenv, SceUID thread_id, Ptr<const char> path, Ptr<const char> title_id,
    SceUInt32 budget_size, Ptr<const char> param, SceUInt32 process_attr, Ptr<const void> option) {
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    if (!thread) {
        LOG_ERROR("LaunchAppByPath4: unknown thread {}", thread_id);
        return static_cast<int>(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }
    LOG_DEBUG("LaunchAppByPath4 path: {}, title: {}, budget: {}, param: {}, process attributes: {}, option: {}",
        appmgr_mount_log_string(thread->cpu->mem, path, 292), appmgr_mount_log_string(thread->cpu->mem, title_id, 32),
        log_hex(budget_size), log_hex(param.address()), log_hex(process_attr), log_hex(option.address()));
    const auto caller = emuenv.kernel.get_process_identity(thread->cpu->mem);
    if (!caller || caller->app_path != "vs0:vsh/shell") {
        LOG_ERROR("LaunchAppByPath4 requires the Shell process");
        return static_cast<int>(SCE_APPMGR_ERROR_INVALID);
    }
    std::string self_path;
    std::string name;
    if (!read_mount_string(thread->cpu->mem, path, 291, self_path)
        || !read_mount_string(thread->cpu->mem, title_id, 31, name) || name.empty()) {
        LOG_ERROR("LaunchAppByPath4 received an invalid path or application name");
        return static_cast<int>(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    }
    const auto separator = self_path.rfind('/');
    const bool ux0_app = appmgr_is_basic_ux0_launch(self_path, name);
    if ((!self_path.starts_with("vs0:") && !ux0_app) || separator == std::string::npos
        || separator <= 4 || separator + 1 == self_path.size() || self_path.find("..") != std::string::npos) {
        LOG_ERROR("LaunchAppByPath4 cannot create a process for '{}'", self_path);
        return static_cast<int>(SCE_APPMGR_ERROR_INVALID_SELF_PATH);
    }
    if (budget_size != 0 || process_attr != 0 || param) {
        LOG_ERROR("LaunchAppByPath4 launch parameters, process attributes and non-default budgets are not implemented");
        return static_cast<int>(SCE_KERNEL_ERROR_NOT_IMPLEMENTED);
    }
    std::optional<AppMgrLaunchOption> launch_option;
    if (option) {
        launch_option = read_appmgr_launch_option(thread->cpu->mem, option);
        if (!launch_option) {
            LOG_ERROR("LaunchAppByPath4 received an invalid option block at {}", log_hex(option.address()));
            return static_cast<int>(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
        }
        LOG_DEBUG("LaunchAppByPath4 option: size {}, flags {}, status {}, param result {}, physical {}, infobar {}, marker {}, origin {}, common dialog {}, extra memory {}",
            log_hex(launch_option->size), log_hex(launch_option->flags), log_hex(launch_option->status.address()),
            log_hex(launch_option->param_result.address()), log_hex(launch_option->physical_contiguous_size), log_hex(launch_option->infobar),
            log_hex(launch_option->launch_argument_marker), log_hex(launch_option->launch_origin),
            launch_option->use_common_dialog, launch_option->enlarge_game_memory_size);
        if (!appmgr_launch_option_supported(*launch_option)) {
            LOG_ERROR("LaunchAppByPath4 requested unsupported non-default options");
            return static_cast<int>(SCE_KERNEL_ERROR_NOT_IMPLEMENTED);
        }
        if (launch_option->infobar || launch_option->launch_argument_marker || launch_option->launch_origin)
            LOG_WARN("LaunchAppByPath4 infobar, argument marker and launch origin are not yet emulated");
    }

    auto &processes = *emuenv.kernel.obj_store.get<AppMgrProcesses>();
    const std::lock_guard process_lock(processes.mutex);
    if (processes.processes.size() >= 19 || processes.shell_events.size() + processes.reserved_shell_events >= 180) {
        LOG_ERROR("LaunchAppByPath4 has no free application slot or Shell event slot");
        return static_cast<int>(SCE_APPMGR_ERROR_BUSY);
    }
    for (const auto &existing : processes.processes) {
        if (existing->title_id == name) {
            LOG_ERROR("LaunchAppByPath4 cannot deliver a second launch to process {} yet", existing->pid);
            return static_cast<int>(SCE_KERNEL_ERROR_NOT_IMPLEMENTED);
        }
    }

    auto process = std::make_unique<SecondaryProcess>();
    process->path = self_path;
    process->title_id = name;
    process->io.app_path = self_path.substr(0, separator);
    process->io.title_id = name;
    process->io.user_id = emuenv.io.user_id;
    process->io.user_name = emuenv.io.user_name;
    process->io.redirect_stdio = emuenv.io.redirect_stdio;
    init_device_paths(process->io, emuenv.vita_fs_path);
    if (ux0_app && !pfsmgr::mount_application_game_data(process->io, emuenv.vita_fs_path)) {
        LOG_ERROR("LaunchAppByPath4 could not mount game data for '{}'", name);
        return static_cast<int>(SCE_KERNEL_ERROR_ERROR);
    }
    const std::string sfo_path = ux0_app ? "app0:sce_sys/param.sfo" : process->io.app_path + "/sce_sys/param.sfo";
    vfs::FileBuffer sfo_buffer;
    SfoFile process_sfo{};
    if (read_file_to_buffer(process->io, sfo_path.c_str(), emuenv.vita_fs_path, sfo_buffer, "LaunchAppByPath4")
        && sfo::load(process_sfo, sfo_buffer)) {
        process->sfo = std::move(process_sfo);
    } else {
        LOG_ERROR("Could not load application parameters from '{}'", sfo_path);
        if (ux0_app)
            return static_cast<int>(SCE_APPMGR_ERROR_APP_PARAM_READ);
    }
    if (ux0_app) {
        const auto savedata_id = appmgr_savedata_id(*process->sfo);
        if (!savedata_id) {
            LOG_ERROR("LaunchAppByPath4 has no valid savedata identifier for '{}'", name);
            return static_cast<int>(SCE_APPMGR_ERROR_APP_PARAM_READ);
        }
        const auto &savedata = *savedata_id;
        const auto savedata_key = string_utils::toupper(savedata);
        if (emuenv.io.user_id == process->io.user_id && string_utils::toupper(emuenv.io.savedata) == savedata_key) {
            LOG_ERROR("LaunchAppByPath4 cannot mount savedata '{}' already used by the primary process", savedata);
            return static_cast<int>(SCE_APPMGR_ERROR_BUSY);
        }
        for (const auto &existing : processes.processes) {
            if (string_utils::toupper(existing->io.savedata) == savedata_key && existing->io.user_id == process->io.user_id) {
                LOG_ERROR("LaunchAppByPath4 cannot mount savedata '{}' already used by process {}", savedata, existing->pid);
                return static_cast<int>(SCE_APPMGR_ERROR_BUSY);
            }
        }
        process->io.savedata = savedata;
        process->io.device_paths.savedata0 = "user/" + process->io.user_id + "/savedata/" + savedata;
        if (!init_savedata_app_path(process->io, emuenv.vita_fs_path,
                [&emuenv] { return app::get_savedata_identity(emuenv.regmgr, emuenv.vita_fs_path); })) {
            LOG_ERROR("LaunchAppByPath4 could not mount savedata for '{}'", name);
            return static_cast<int>(SCE_KERNEL_ERROR_ERROR);
        }
        process->apputil.io = &process->io;
    }
    std::array<uint8_t, 16> license_key{};
    if (ux0_app && name.starts_with("PCS")) {
        std::string content_id;
        if (sfo::get_data_by_key(content_id, *process->sfo, "CONTENT_ID")
            && content_id.size() == 36
            && std::ranges::all_of(content_id, [](unsigned char ch) {
                   return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
               })) {
            SceNpDrmLicense license{};
            const auto license_path = emuenv.vita_fs_path / "ux0" / "license" / name / (content_id + ".rif");
            if (open_license(license_path, license))
                std::copy(std::begin(license.key), std::end(license.key), license_key.begin());
            else
                LOG_WARN("LaunchAppByPath4 could not read the license for '{}'; using the default SELF key", name);
        } else {
            LOG_WARN("LaunchAppByPath4 has no valid content ID for '{}'; using the default SELF key", name);
        }
    }
    if (!init(process->mem, emuenv.mem.use_page_table)) {
        LOG_ERROR("LaunchAppByPath4 could not initialize memory for '{}'", name);
        return static_cast<int>(SCE_KERNEL_ERROR_NO_MEMORY);
    }

    const auto cleanup = [&] {
        emuenv.kernel.stop_threads_in_address_space(process->mem);
        if (process->main_module_id > 0 && unload_module(emuenv, process->mem, process->main_module_id) < 0) {
            LOG_ERROR("LaunchAppByPath4 could not unload failed process '{}'; retaining its memory until session shutdown", name);
            processes.processes.push_back(std::move(process));
            return;
        }
        if (!emuenv.kernel.release_thread_address_space(process->mem)) {
            LOG_ERROR("LaunchAppByPath4 could not release failed process '{}'; retaining its memory until session shutdown", name);
            processes.processes.push_back(std::move(process));
        }
    };

    if (!emuenv.kernel.register_process_identity(process->mem, { process->io.app_path })
        || !init_exported_vars(emuenv, process->mem)) {
        LOG_ERROR("LaunchAppByPath4 could not initialize process '{}'", name);
        cleanup();
        return static_cast<int>(SCE_KERNEL_ERROR_NO_MEMORY);
    }
    process->main_module_id = load_module(emuenv, process->mem, process->io,
        ux0_app ? "app0:eboot.bin" : self_path, ux0_app ? license_key.data() : nullptr);
    if (process->main_module_id < 0) {
        const int error = process->main_module_id;
        LOG_ERROR("LaunchAppByPath4 could not load '{}'", self_path);
        cleanup();
        return error;
    }
    const auto module = emuenv.kernel.find_module(process->mem, process->main_module_id);
    const auto identity = emuenv.kernel.get_process_identity(process->mem);
    if (!module || !identity || !module->info.start_entry) {
        LOG_ERROR("LaunchAppByPath4 could not prepare the main module for '{}'", name);
        cleanup();
        return static_cast<int>(SCE_KERNEL_ERROR_NOT_IMPLEMENTED);
    }
    auto main_thread = emuenv.kernel.create_process_main_thread(process->mem, name.c_str(), module->info.start_entry);
    if (!main_thread) {
        LOG_ERROR("LaunchAppByPath4 could not create the main thread for '{}'", name);
        cleanup();
        return static_cast<int>(SCE_KERNEL_ERROR_NO_MEMORY);
    }
    process->pid = identity->pid;
    process->main_thread_id = main_thread->id;
    process->app_id = static_cast<SceUID>((processes.app_generation << 16) | (2 * (processes.processes.size() + 1) + 2));
    processes.app_generation = (processes.app_generation + 1) & 0x7fff;
    const SceUID pid = process->pid;
    const SceUID app_id = process->app_id;
    process->launch_status = make_appmgr_launch_status(name, caller->pid, pid, app_id,
        launch_option ? &*launch_option : nullptr);
    const char *diagnostic = std::getenv("VITA3K_EXPERIMENTAL_START_SECONDARY");
    if (diagnostic && std::strcmp(diagnostic, "1") == 0) {
        const SceInt32 result = emuenv.kernel.start_process_main_thread(main_thread, {});
        if (result < 0) {
            LOG_ERROR("Could not start secondary process {}: {}", pid, log_hex(result));
            process->app_id = 0;
            main_thread.reset();
            cleanup();
            return result;
        }
        process->state = SecondaryProcess::State::started;
        LOG_WARN("Started secondary process {} in diagnostic mode; unsafe HLE imports stop its thread", pid);
    } else {
        LOG_INFO("Created process {} for '{}' as app {}; waiting for Shell activation", pid, name, log_hex(app_id));
    }
    if (launch_option && launch_option->status) {
        const auto status = secondary_status(*process);
        if (!write_appmgr_launch_status(thread->cpu->mem, launch_option->status, status.status)) {
            LOG_ERROR("LaunchAppByPath4 received an invalid status output at {}", log_hex(launch_option->status.address()));
            process->app_id = 0;
            main_thread.reset();
            cleanup();
            return static_cast<int>(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
        }
    }
    processes.processes.push_back(std::move(process));
    processes.shell_events.push_back({ 2, pid, app_id, {} });
    LOG_DEBUG("Queued Shell app-loaded event for process {} and app {}", pid, log_hex(app_id));
    return pid;
}

EXPORT(int, _sceAppMgrLaunchAppByPath4, Ptr<const char> path, Ptr<const char> title_id,
    SceUInt32 budget_size, Ptr<const AppMgrLaunchAppByPathArgs> arguments) {
    TRACY_FUNC(_sceAppMgrLaunchAppByPath4, path, title_id, budget_size, arguments);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    if (!thread) {
        LOG_ERROR("LaunchAppByPath4: unknown thread {}", thread_id);
        return static_cast<int>(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }
    const auto args = read_appmgr_launch_path_args(thread->cpu->mem, arguments);
    if (!args)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    return appmgr_launch_app_by_path(emuenv, thread_id, path, title_id, budget_size, args->param, args->process_attr, args->option);
}

EXPORT(int, _sceAppMgrLaunchAppByUri) {
    TRACY_FUNC(_sceAppMgrLaunchAppByUri);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchAppByUri2) {
    TRACY_FUNC(_sceAppMgrLaunchAppByUri2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLaunchVideoStreamingApp) {
    TRACY_FUNC(_sceAppMgrLaunchVideoStreamingApp);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceAppMgrLoadExec, const char *appPath, Ptr<char> const argv[], const SceAppMgrLoadExecOptParam *optParam) {
    TRACY_FUNC(_sceAppMgrLoadExec, appPath, argv, optParam);
    if (optParam)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID);
    if (const SceInt32 error = emuenv.kernel.check_primary_address_space_access(thread_id, "LoadExec"))
        return RET_ERROR(error);

    // Create exec path
    auto exec_path = static_cast<std::string>(appPath);
    if (exec_path.contains("app0:/"))
        exec_path.erase(0, 6);
    else
        exec_path.erase(0, 5);

    LOG_INFO("sceAppMgrLoadExec run self: {}", appPath);

    // Load exec executable
    vfs::FileBuffer exec_buffer;
    const auto mounted_exec_path = "app0:/" + exec_path;
    const bool read_exec = emuenv.io.app_game_base_mount.empty()
        ? vfs::read_app_file(exec_buffer, emuenv.vita_fs_path, emuenv.io.app_path, exec_path, emuenv.io.title_id)
        : read_file_to_buffer(emuenv.io, mounted_exec_path.c_str(), emuenv.vita_fs_path, exec_buffer, export_name);
    if (read_exec) {
        std::vector<std::string> exec_argv;
        if (argv && argv->get(emuenv.mem)) {
            size_t args = 0;
            for (auto i = 0; argv[i]; i++) {
                const char *arg = argv[i].get(emuenv.mem);
                LOG_INFO("sceAppMgrLoadExec run with argument at {}: {}", i, arg);
                args += strlen(arg);
                exec_argv.emplace_back(arg);
            }

            if (args > 1024)
                return RET_ERROR(SCE_APPMGR_ERROR_TOO_LONG_ARGV);
        }

        if (const SceInt32 error = emuenv.kernel.request_process_exit(thread_id, 0, AppLaunchRequest{ .app_path = emuenv.io.app_path, .self_path = std::move(exec_path), .argv = std::move(exec_argv), .reason = AppLaunchReason::LoadExec }))
            return RET_ERROR(error);

        return SCE_KERNEL_OK;
    }

    return RET_ERROR(SCE_APPMGR_ERROR_INVALID_SELF_PATH);
}

EXPORT(int, _sceAppMgrLoadSaveDataSystemFile) {
    TRACY_FUNC(_sceAppMgrLoadSaveDataSystemFile);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLoopBackFormat) {
    TRACY_FUNC(_sceAppMgrLoopBackFormat);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrLoopBackMount) {
    TRACY_FUNC(_sceAppMgrLoopBackMount);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceAppMgrMmsMount, SceInt32 id, char *mount_point) {
    TRACY_FUNC(_sceAppMgrMmsMount, id, mount_point);
    switch (id) {
    case 0x190:
        strcpy(mount_point, "ux0:mms/photo");
        break;
    case 0x191:
        strcpy(mount_point, "ux0:mms/music");
        break;
    case 0x192:
        strcpy(mount_point, "ux0:mms/video");
        break;
    default:
        LOG_ERROR("Unknown id: {}", log_hex(id));
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    }

    return STUBBED("using strcpy");
}

EXPORT(int, _sceAppMgrOverwriteLaunchParamForShell) {
    TRACY_FUNC(_sceAppMgrOverwriteLaunchParamForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrPeekLaunchParamForShell) {
    TRACY_FUNC(_sceAppMgrPeekLaunchParamForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrPhotoMount) {
    TRACY_FUNC(_sceAppMgrPhotoMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrPhotoUmount) {
    TRACY_FUNC(_sceAppMgrPhotoUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrPspSaveDataGetParams) {
    TRACY_FUNC(_sceAppMgrPspSaveDataGetParams);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrPspSaveDataRead) {
    TRACY_FUNC(_sceAppMgrPspSaveDataRead);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrPspSaveDataRootMount) {
    TRACY_FUNC(_sceAppMgrPspSaveDataRootMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrReceiveEvent, SceAppMgrEvent *event) {
    TRACY_FUNC(_sceAppMgrReceiveEvent, event);
    if (!event)
        return RET_ERROR(SCE_APPMGR_ERROR_NULL_POINTER);
    STUBBED("Only launch parameter events are supported");
    auto *events = emuenv.kernel.obj_store.get<AppMgrEvents>();
    if (!events->launch_param_pending.exchange(false)) {
        event->event = 0;
        return RET_ERROR(SCE_APPMGR_ERROR_NO_EVENT);
    }
    *event = {};
    event->event = 0x20000004;
    event->app_id = -1;
    return 0;
}

EXPORT(int, _sceAppMgrReceiveEventNum, SceUInt32 *eventNum) {
    TRACY_FUNC(_sceAppMgrReceiveEventNum, eventNum);
    if (!eventNum)
        return RET_ERROR(SCE_APPMGR_ERROR_NULL_POINTER);
    auto *events = emuenv.kernel.obj_store.get<AppMgrEvents>();
    *eventNum = events->launch_param_pending.load() ? 1 : 0;
    STUBBED("Only launch parameter events are supported");
    return 0;
}

EXPORT(int, _sceAppMgrReceiveNotificationRequestForShell) {
    TRACY_FUNC(_sceAppMgrReceiveNotificationRequestForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrReceiveShellEvent, Ptr<SceAppMgrShellEvent> event) {
    TRACY_FUNC(_sceAppMgrReceiveShellEvent, event);
    return appmgr_receive_shell_event(emuenv, thread_id, event);
}

EXPORT(int, _sceAppMgrReceiveSystemEvent, SceAppMgrSystemEvent *systemEvent) {
    TRACY_FUNC(_sceAppMgrReceiveSystemEvent, systemEvent);
    systemEvent->systemEvent = SCE_APPMGR_SYSTEMEVENT_ON_RESUME;
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataAddMount) {
    TRACY_FUNC(_sceAppMgrSaveDataAddMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataDataRemove) {
    TRACY_FUNC(_sceAppMgrSaveDataDataRemove);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataDataRemove2) {
    TRACY_FUNC(_sceAppMgrSaveDataDataRemove2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataDataSave) {
    TRACY_FUNC(_sceAppMgrSaveDataDataSave);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataDataSave2) {
    TRACY_FUNC(_sceAppMgrSaveDataDataSave2);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataGetQuota) {
    TRACY_FUNC(_sceAppMgrSaveDataGetQuota);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataMount) {
    TRACY_FUNC(_sceAppMgrSaveDataMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotCreate) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotCreate);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotDelete) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotDelete);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotFileClose) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotFileClose);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotFileGetParam) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotFileGetParam);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotFileOpen) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotFileOpen);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotGetParam) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotGetParam);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotGetStatus) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotGetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotInit) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotInit);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotSetParam) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotSetParam);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataSlotSetStatus) {
    TRACY_FUNC(_sceAppMgrSaveDataSlotSetStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSaveDataUmount) {
    TRACY_FUNC(_sceAppMgrSaveDataUmount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSendNotificationRequest) {
    TRACY_FUNC(_sceAppMgrSendNotificationRequest);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSendParam) {
    TRACY_FUNC(_sceAppMgrSendParam);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSendSystemEvent) {
    TRACY_FUNC(_sceAppMgrSendSystemEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSendSystemEvent2) {
    TRACY_FUNC(_sceAppMgrSendSystemEvent2);
    return UNIMPLEMENTED();
}

int appmgr_set_back_render_port_owner(EmuEnvState &emuenv, SceUID thread_id, SceInt32 app_id) {
    if (!is_shell_caller(emuenv, thread_id))
        return static_cast<int>(SCE_APPMGR_ERROR_SHELL_ONLY);
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes) {
        LOG_ERROR("Cannot set back render port owner without AppMgr process state");
        return static_cast<int>(SCE_KERNEL_ERROR_ERROR);
    }
    const std::lock_guard activation_lock(processes->activation_mutex);
    const std::lock_guard lock(processes->mutex);
    SceUID pid = 0;
    if (app_id != -1) {
        const auto it = std::find_if(processes->processes.begin(), processes->processes.end(),
            [app_id](const auto &process) { return process->app_id && process->app_id == app_id; });
        if (it == processes->processes.end()) {
            LOG_ERROR("Cannot set back render port owner: app {} was not found", log_hex(app_id));
            return static_cast<int>(SCE_APPMGR_ERROR_APP_NOT_FOUND);
        }
        pid = (*it)->pid;
    }
    if (display::select_process_frame(emuenv.display, pid))
        emuenv.renderer->should_display = true;
    return 0;
}

EXPORT(int, _sceAppMgrSetBackRenderPortOwner, SceInt32 app_id) {
    TRACY_FUNC(_sceAppMgrSetBackRenderPortOwner, app_id);
    return appmgr_set_back_render_port_owner(emuenv, thread_id, app_id);
}

EXPORT(int, _sceAppMgrSetBgmProxyApp) {
    TRACY_FUNC(_sceAppMgrSetBgmProxyApp);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetNetworkDisconnectionWarningDialogState) {
    TRACY_FUNC(_sceAppMgrSetNetworkDisconnectionWarningDialogState);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetPowerSaveMode) {
    TRACY_FUNC(_sceAppMgrSetPowerSaveMode);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetRecommendedScreenOrientationForShell) {
    TRACY_FUNC(_sceAppMgrSetRecommendedScreenOrientationForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetShellScreenOrientation) {
    TRACY_FUNC(_sceAppMgrSetShellScreenOrientation);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetSystemDataFile) {
    TRACY_FUNC(_sceAppMgrSetSystemDataFile);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSetSystemDataFilePlayReady) {
    TRACY_FUNC(_sceAppMgrSetSystemDataFilePlayReady);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSystemParamDateTimeGetConf) {
    TRACY_FUNC(_sceAppMgrSystemParamDateTimeGetConf);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSystemParamGetInt) {
    TRACY_FUNC(_sceAppMgrSystemParamGetInt);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrSystemParamGetString) {
    TRACY_FUNC(_sceAppMgrSystemParamGetString);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrThemeDataMount, Ptr<const char> contentId, Ptr<const void> option1, Ptr<const void> option2, Ptr<char> mountPoint) {
    TRACY_FUNC(_sceAppMgrThemeDataMount, contentId.address(), option1.address(), option2.address(), mountPoint.address());
    std::string id;
    if (!read_mount_string(emuenv.mem, contentId, 0x7f, id) || !valid_mount_output(emuenv.mem, mountPoint)
        || id.empty() || id == "." || id == ".." || id.find_first_of("\\/:") != std::string::npos)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    if (option1 || option2) {
        LOG_WARN("ThemeDataMount with options is not implemented");
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    }
    std::string mount_point;
    bool is_protected = false;
    const auto directory = emuenv.vita_fs_path / "ux0" / "theme" / fs_utils::utf8_to_path(id);
    if (!pfsmgr::mount_theme_data(emuenv.io, directory, mount_point, is_protected)) {
        LOG_WARN("Cannot mount theme data for '{}'", id);
        return RET_ERROR(SCE_ERROR_ERRNO_ENOENT);
    }
    for (Address i = 0; i < 16; ++i)
        *Ptr<char>(mountPoint.address() + i).get(emuenv.mem) = i < mount_point.size() ? mount_point[i] : '\0';
    LOG_INFO("Mounted theme data for '{}' at '{}'", id, mount_point);
    return 0;
}

EXPORT(int, _sceAppMgrTrophyMount) {
    TRACY_FUNC(_sceAppMgrTrophyMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrTrophyMountById) {
    TRACY_FUNC(_sceAppMgrTrophyMountById);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrUmount, Ptr<const char> mountPoint) {
    TRACY_FUNC(_sceAppMgrUmount, mountPoint.address());
    std::string mount_point;
    if (!read_mount_string(emuenv.mem, mountPoint, 15, mount_point))
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER);
    if (pfsmgr::unmount_game_data(emuenv.io, mount_point)) {
        LOG_INFO("Unmounted game data '{}'", mount_point);
        return 0;
    }
    if (pfsmgr::unmount_appmeta(emuenv.io, mount_point)) {
        LOG_INFO("Unmounted appmeta '{}'", mount_point);
        return 0;
    }
    if (pfsmgr::unmount_theme_data(emuenv.io, mount_point)) {
        LOG_INFO("Unmounted theme data '{}'", mount_point);
        return 0;
    }
    if (mount_point == "ux0:picture" || mount_point == "friend0:" || mount_point == "psnmsg0:"
        || mount_point == "near0:" || mount_point == "ux0:music" || mount_point == "ux0:calendar"
        || mount_point == "ux0:video" || mount_point == "sqlite0:/" || mount_point == "attach0:/"
        || mount_point == "ux0:pspemu/" || mount_point == "checkout0:/" || mount_point == "webbrowser0:/"
        || mount_point == "ux0:mms/photo" || mount_point == "ux0:mms/music" || mount_point == "ux0:mms/video")
        return STUBBED("Unmount of legacy static path redirection");
    LOG_WARN("Unknown or unsupported AppMgr mount '{}'", mount_point);
    return RET_ERROR(SCE_ERROR_ERRNO_ENOENT);
}

EXPORT(int, _sceAppMgrUmountByPid) {
    TRACY_FUNC(_sceAppMgrUmountByPid);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrUpdateSaveDataParam) {
    TRACY_FUNC(_sceAppMgrUpdateSaveDataParam);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrWorkDirMount) {
    TRACY_FUNC(_sceAppMgrWorkDirMount);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceAppMgrWorkDirMountById) {
    TRACY_FUNC(_sceAppMgrWorkDirMountById);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAcquireBgmPort) {
    TRACY_FUNC(sceAppMgrAcquireBgmPort);
    return CALL_EXPORT(sceAppMgrAcquireBgmPortWithPriority, 256);
}

EXPORT(int, sceAppMgrAcquireBgmPortForMusicPlayer) {
    TRACY_FUNC(sceAppMgrAcquireBgmPortForMusicPlayer);
    return CALL_EXPORT(sceAppMgrAcquireBgmPortWithPriority, 130);
}

EXPORT(int, sceAppMgrAcquireBgmPortWithPriority, int priority) {
    TRACY_FUNC(sceAppMgrAcquireBgmPortWithPriority, priority);
    if (priority < 0)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID);
    const auto pid = emuenv.kernel.get_thread_process_id(thread_id);
    if (!pid)
        return RET_ERROR(SCE_KERNEL_ERROR_ERROR);
    auto *bgm = emuenv.kernel.obj_store.get<AppMgrBgm>();
    if (!bgm) {
        LOG_ERROR("{}: BGM state is not initialized", export_name);
        return RET_ERROR(SCE_KERNEL_ERROR_ERROR);
    }
    try {
        if (!bgm->acquire(*pid, priority))
            return RET_ERROR(SCE_APPMGR_ERROR_BGM_PORT_BUSY);
    } catch (const std::bad_alloc &) {
        LOG_ERROR("{}: cannot allocate BGM state for process {}", export_name, *pid);
        return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
    }
    return STUBBED("BGM priority arbitration, permissions and audio adoption are not emulated");
}

EXPORT(int, sceAppMgrAcquireBtrm) {
    TRACY_FUNC(sceAppMgrAcquireBtrm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAcquireSoundOutExclusive) {
    TRACY_FUNC(sceAppMgrAcquireSoundOutExclusive);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrAcquireSoundOutExclusive2) {
    TRACY_FUNC(sceAppMgrAcquireSoundOutExclusive2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrActivateApp, SceInt32 app_id) {
    TRACY_FUNC(sceAppMgrActivateApp, app_id);
    if (!is_shell_caller(emuenv, thread_id))
        return RET_ERROR(SCE_APPMGR_ERROR_NOT_SHELL);
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes) {
        LOG_ERROR("Cannot activate an app without AppMgr process state");
        return RET_ERROR(SCE_KERNEL_ERROR_ERROR);
    }
    const std::lock_guard activation_lock(processes->activation_mutex);
    SecondaryProcess *process = nullptr;
    {
        const std::lock_guard lock(processes->mutex);
        for (const auto &candidate : processes->processes) {
            if (candidate->app_id == app_id && app_id != 0) {
                process = candidate.get();
                break;
            }
        }
        if (!process)
            return RET_ERROR(SCE_APPMGR_ERROR_APP_NOT_FOUND);
        if (process->state == SecondaryProcess::State::starting)
            return RET_ERROR(SCE_APPMGR_ERROR_BUSY);
        if (process->state == SecondaryProcess::State::started) {
            for (const auto &candidate : processes->processes)
                candidate->activated = candidate.get() == process;
            if (display::select_process_frame(emuenv.display, process->pid))
                emuenv.renderer->should_display = true;
            return STUBBED("Application lifecycle and device arbitration are incomplete");
        }
        if (processes->shell_events.size() + processes->reserved_shell_events >= 180) {
            LOG_ERROR("Cannot activate app {}: Shell event queue is full", log_hex(app_id));
            return RET_ERROR(SCE_APPMGR_ERROR_BUSY);
        }
        ++processes->reserved_shell_events;
        process->state = SecondaryProcess::State::starting;
    }
    const auto main_thread = emuenv.kernel.get_thread(process->main_thread_id);
    const SceInt32 result = emuenv.kernel.start_process_main_thread(main_thread, {});
    {
        const std::lock_guard lock(processes->mutex);
        --processes->reserved_shell_events;
        process->state = result < 0 ? SecondaryProcess::State::created : SecondaryProcess::State::started;
        if (result >= 0) {
            for (const auto &candidate : processes->processes)
                candidate->activated = candidate.get() == process;
            processes->shell_events.push_back({ 18, 0, 0, {} });
            LOG_DEBUG("Queued Shell app-running-status-changed event for process {} and app {}", process->pid, log_hex(app_id));
            if (display::select_process_frame(emuenv.display, process->pid))
                emuenv.renderer->should_display = true;
        }
    }
    if (result < 0) {
        LOG_ERROR("Could not activate app {} (process {}): {}", log_hex(app_id), process->pid, log_hex(result));
        return result;
    }
    LOG_WARN("Shell activated app {} (process {}); application lifecycle and device arbitration remain incomplete", log_hex(app_id), process->pid);
    return 0;
}

EXPORT(int, sceAppMgrDeactivateApp, SceInt32 app_id) {
    TRACY_FUNC(sceAppMgrDeactivateApp, app_id);
    if (!is_shell_caller(emuenv, thread_id))
        return RET_ERROR(SCE_APPMGR_ERROR_NOT_SHELL);
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes)
        return RET_ERROR(SCE_KERNEL_ERROR_ERROR);
    const std::lock_guard activation_lock(processes->activation_mutex);
    const std::lock_guard lock(processes->mutex);
    const auto it = std::find_if(processes->processes.begin(), processes->processes.end(),
        [app_id](const auto &process) { return app_id != 0 && process->app_id == app_id; });
    if (it == processes->processes.end())
        return RET_ERROR(SCE_APPMGR_ERROR_APP_NOT_FOUND);
    if ((*it)->state == SecondaryProcess::State::starting)
        return RET_ERROR(SCE_APPMGR_ERROR_BUSY);
    if (!(*it)->activated)
        return RET_ERROR(SCE_APPMGR_ERROR_STATE);
    (*it)->activated = false;
    return STUBBED("Application lifecycle and device arbitration are incomplete");
}

EXPORT(int, sceAppMgrDeclareSystemChatApp) {
    TRACY_FUNC(sceAppMgrDeclareSystemChatApp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDestroyAppByAppId) {
    TRACY_FUNC(sceAppMgrDestroyAppByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDestroyOtherApp) {
    TRACY_FUNC(sceAppMgrDestroyOtherApp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDestroyOtherAppByAppIdForShell) {
    TRACY_FUNC(sceAppMgrDestroyOtherAppByAppIdForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDestroyOtherAppByPidForShell) {
    TRACY_FUNC(sceAppMgrDestroyOtherAppByPidForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrDump) {
    TRACY_FUNC(sceAppMgrDump);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrEnableCoredumpForTest) {
    TRACY_FUNC(sceAppMgrEnableCoredumpForTest);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrEnableDuckingOnSystemChat) {
    TRACY_FUNC(sceAppMgrEnableDuckingOnSystemChat);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrEnablePrioritizingSystemChat) {
    TRACY_FUNC(sceAppMgrEnablePrioritizingSystemChat);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrExitToLiveboardForGameApp) {
    TRACY_FUNC(sceAppMgrExitToLiveboardForGameApp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrFinishCoredumpForShell) {
    TRACY_FUNC(sceAppMgrFinishCoredumpForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetAppIdByAppId) {
    TRACY_FUNC(sceAppMgrGetAppIdByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetExtraAppParam) {
    TRACY_FUNC(sceAppMgrGetExtraAppParam);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGetProcessIdByAppIdForShell, SceInt32 app_id) {
    TRACY_FUNC(sceAppMgrGetProcessIdByAppIdForShell, app_id);
    if (!is_shell_caller(emuenv, thread_id))
        return RET_ERROR(SCE_APPMGR_ERROR_NOT_SHELL);
    if (app_id == -2) {
        const auto pid = emuenv.kernel.get_thread_process_id(thread_id);
        if (!pid)
            return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
        return *pid;
    }
    if (app_id < -1) {
        LOG_ERROR("{}: special app ID {} is not implemented", export_name, app_id);
        return RET_ERROR(SCE_KERNEL_ERROR_NOT_IMPLEMENTED);
    }
    const auto app = find_secondary_app_by_id(emuenv, app_id);
    if (!app)
        return RET_ERROR(SCE_APPMGR_ERROR_APP_NOT_FOUND);
    return app->status.process_id;
}

EXPORT(int, sceAppMgrGetSystemDataFile) {
    TRACY_FUNC(sceAppMgrGetSystemDataFile);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGrowMemory) {
    TRACY_FUNC(sceAppMgrGrowMemory);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrGrowMemory3) {
    TRACY_FUNC(sceAppMgrGrowMemory3);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsDevelopmentMode) {
    TRACY_FUNC(sceAppMgrIsDevelopmentMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsGameBudgetAppPresent) {
    TRACY_FUNC(sceAppMgrIsGameBudgetAppPresent);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsGameProgram) {
    TRACY_FUNC(sceAppMgrIsGameProgram);
    return 0;
}

EXPORT(int, sceAppMgrIsNonGameProgram) {
    TRACY_FUNC(sceAppMgrIsNonGameProgram);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsOtherAppPresent) {
    TRACY_FUNC(sceAppMgrIsOtherAppPresent);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsPidShellAndCrashed) {
    TRACY_FUNC(sceAppMgrIsPidShellAndCrashed);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrIsPsNowClient) {
    TRACY_FUNC(sceAppMgrIsPsNowClient);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrLaunchAppCancel) {
    TRACY_FUNC(sceAppMgrLaunchAppCancel);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAppMgrLoadSafeMemory, void *buffer, SceSize size, SceOff offset) {
    TRACY_FUNC(sceAppMgrLoadSafeMemory, buffer, size, offset);
    if (!buffer)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_POINTER);
    if (offset < 0 || static_cast<uint64_t>(offset) + size > SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER2);
    const auto result = execute_savedata_safe_memory(emuenv, buffer, size, offset, export_name, false);
    if (result < 0)
        return RET_ERROR(result);
    return result > 0 ? size : 0;
}

EXPORT(int, sceAppMgrNotifyLiveBoardModeForShell) {
    TRACY_FUNC(sceAppMgrNotifyLiveBoardModeForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrQuitApp) {
    TRACY_FUNC(sceAppMgrQuitApp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrQuitForNonSuspendableApp) {
    TRACY_FUNC(sceAppMgrQuitForNonSuspendableApp);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReceiveShellEventNum) {
    TRACY_FUNC(sceAppMgrReceiveShellEventNum);
    if (!is_shell_caller(emuenv, thread_id))
        return RET_ERROR(SCE_APPMGR_ERROR_NOT_SHELL);
    auto *processes = emuenv.kernel.obj_store.get<AppMgrProcesses>();
    if (!processes) {
        LOG_ERROR("Cannot count Shell events without AppMgr process state");
        return RET_ERROR(SCE_KERNEL_ERROR_ERROR);
    }
    const std::lock_guard lock(processes->mutex);
    return static_cast<int>(processes->shell_events.size());
}

EXPORT(int, sceAppMgrReleaseBgmPort) {
    TRACY_FUNC(sceAppMgrReleaseBgmPort);
    const auto pid = emuenv.kernel.get_thread_process_id(thread_id);
    if (!pid)
        return RET_ERROR(SCE_KERNEL_ERROR_ERROR);
    auto *bgm = emuenv.kernel.obj_store.get<AppMgrBgm>();
    if (!bgm) {
        LOG_ERROR("{}: BGM state is not initialized", export_name);
        return RET_ERROR(SCE_KERNEL_ERROR_ERROR);
    }
    bgm->release(*pid);
    return STUBBED("BGM audio adoption is not emulated");
}

EXPORT(int, sceAppMgrReleaseBtrm) {
    TRACY_FUNC(sceAppMgrReleaseBtrm);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReleaseSoundOutExclusive) {
    TRACY_FUNC(sceAppMgrReleaseSoundOutExclusive);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReleaseSoundOutExclusive2) {
    TRACY_FUNC(sceAppMgrReleaseSoundOutExclusive2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReleaseSoundOutExclusive3) {
    TRACY_FUNC(sceAppMgrReleaseSoundOutExclusive3);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrRestoreBgmSettingForShell) {
    TRACY_FUNC(sceAppMgrRestoreBgmSettingForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrRestoreDisplaySettingForShell) {
    TRACY_FUNC(sceAppMgrRestoreDisplaySettingForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrResumeBgAppByShell) {
    TRACY_FUNC(sceAppMgrResumeBgAppByShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrReturnLiveAreaOperationResultForShell) {
    TRACY_FUNC(sceAppMgrReturnLiveAreaOperationResultForShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSaveDataGetCachedRequiredSizeKiB) {
    TRACY_FUNC(sceAppMgrSaveDataGetCachedRequiredSizeKiB);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAppMgrSaveSafeMemory, const void *buffer, SceSize size, SceOff offset) {
    TRACY_FUNC(sceAppMgrSaveSafeMemory, buffer, size, offset);
    if (!buffer)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_POINTER);
    if (offset < 0 || static_cast<uint64_t>(offset) + size > SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID_PARAMETER2);
    const auto result = execute_savedata_safe_memory(emuenv, const_cast<void *>(buffer), size, offset, export_name, true);
    return result < 0 ? RET_ERROR(result) : static_cast<SceInt32>(size);
}

EXPORT(int, sceAppMgrSendLiveBoardMode) {
    TRACY_FUNC(sceAppMgrSendLiveBoardMode);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetAppProtectionModeOnMemoryShortage) {
    TRACY_FUNC(sceAppMgrSetAppProtectionModeOnMemoryShortage);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetBgmSubPriority, int enabled, int priority) {
    TRACY_FUNC(sceAppMgrSetBgmSubPriority, enabled, priority);
    if (priority < 0)
        return RET_ERROR(SCE_APPMGR_ERROR_INVALID);
    const auto pid = emuenv.kernel.get_thread_process_id(thread_id);
    if (!pid)
        return RET_ERROR(SCE_KERNEL_ERROR_ERROR);
    auto *bgm = emuenv.kernel.obj_store.get<AppMgrBgm>();
    if (!bgm) {
        LOG_ERROR("{}: BGM state is not initialized", export_name);
        return RET_ERROR(SCE_KERNEL_ERROR_ERROR);
    }
    try {
        bgm->set_sub_priority(*pid, enabled != 0, priority);
    } catch (const std::bad_alloc &) {
        LOG_ERROR("{}: cannot allocate BGM state for process {}", export_name, *pid);
        return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
    }
    return STUBBED("BGM priority arbitration, permissions and audio adoption are not emulated");
}

EXPORT(int, sceAppMgrSetBgmSubPriorityForSystemChat, int enabled) {
    TRACY_FUNC(sceAppMgrSetBgmSubPriorityForSystemChat, enabled);
    return CALL_EXPORT(sceAppMgrSetBgmSubPriority, enabled, 194);
}

EXPORT(int, sceAppMgrSetDisplayMergeConf) {
    TRACY_FUNC(sceAppMgrSetDisplayMergeConf);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetFakeSettingBug51800) {
    TRACY_FUNC(sceAppMgrSetFakeSettingBug51800);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetInfobarState) {
    TRACY_FUNC(sceAppMgrSetInfobarState);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetInfobarStateForCommonDialog) {
    TRACY_FUNC(sceAppMgrSetInfobarStateForCommonDialog);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetInfobarStateForShellByAppId) {
    TRACY_FUNC(sceAppMgrSetInfobarStateForShellByAppId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetRecommendedScreenOrientationActivated) {
    TRACY_FUNC(sceAppMgrSetRecommendedScreenOrientationActivated);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetSystemImposeState) {
    TRACY_FUNC(sceAppMgrSetSystemImposeState);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSetSystemImposeState2) {
    TRACY_FUNC(sceAppMgrSetSystemImposeState2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSuspendBgAppByShell) {
    TRACY_FUNC(sceAppMgrSuspendBgAppByShell);
    return UNIMPLEMENTED();
}

EXPORT(int, sceAppMgrSuspendUntilActivated) {
    TRACY_FUNC(sceAppMgrSuspendUntilActivated);
    return UNIMPLEMENTED();
}
