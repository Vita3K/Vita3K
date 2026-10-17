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

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#include <cpu/common.h>
#include <kernel/state.h>
#include <kernel/sync_primitives.h>
#include <mem/functions.h>

#include <kernel/thread/thread_state.h>

#include <cpu/state.h>
#include <mem/ptr.h>
#include <util/lock_and_find.h>
#include <util/log.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_thread.h>

#include <cstring>
#include <limits>

int CorenumAllocator::new_corenum() {
    const std::lock_guard<std::mutex> guard(lock);

    uint32_t size = 1;
    return alloc.allocate_from(0, size);
}

std::shared_ptr<SecondaryMemblockState> KernelState::secondary_memblocks(MemState &mem, std::shared_ptr<SecondaryMemblockState> candidate) {
    const std::lock_guard<std::mutex> lock(mutex);
    const auto it = thread_address_spaces.find(&mem);
    if (&mem == primary_address_space || it == thread_address_spaces.end() || !it->second.identity || it->second.stopping)
        return {};
    if (!it->second.memblocks)
        it->second.memblocks = std::move(candidate);
    return it->second.memblocks;
}

void CorenumAllocator::free_corenum(const int num) {
    const std::lock_guard<std::mutex> guard(lock);
    alloc.free(num, 1);
}

void CorenumAllocator::set_max_core_count(const std::size_t max) {
    const std::lock_guard<std::mutex> guard(lock);
    alloc.set_maximum(max);
}

// TODO implement cross platform debug thread name setter and eliminate SDL thread
struct ThreadParams {
    KernelState *kernel = nullptr;
    // Passed in rather than looked up, since the guest may delete the thread before it starts
    ThreadStatePtr thread;
    SDL_Semaphore *host_may_destroy_params = nullptr;
};

static int SDLCALL thread_function(void *data) {
    assert(data != nullptr);
    ThreadParams params = *static_cast<const ThreadParams *>(data);
    SDL_SignalSemaphore(params.host_may_destroy_params);
    ThreadStatePtr thread = std::move(params.thread);
#ifdef TRACY_ENABLE
    if (thread->name[0] != '\0') {
        tracy::SetThreadName(thread->name);
    } else {
        std::string th_name = "TID:" + std::to_string(thread->id);
        tracy::SetThreadName(th_name.c_str());
    }
#endif

    thread->run_loop();
    const uint32_t r0 = thread->cpu->get_reg(0);
    const SceUID id = thread->id;
    const int processor_id = thread->cpu->processor_id;
    // release our reference first so the erase below destroys the ThreadState before process_exit() is woken
    thread.reset();

    {
        std::lock_guard<std::mutex> lock(params.kernel->mutex);
        params.kernel->threads.erase(id);
        params.kernel->corenum_allocator.free_corenum(processor_id);
        params.kernel->thread_deleted_cond.notify_all();
    }

    return r0;
}

KernelState::KernelState()
    : debugger(*this) {
}

ModuleBindingView KernelState::module_bindings(const MemState &mem) {
    if (&mem == primary_address_space)
        return { export_nids, export_nids_by_lib, export_nid_owners, func_binding_infos, var_binding_infos, variable_stubs, variable_exports };

    auto &tables = secondary_module_bindings[&mem];
    return { tables.exports, tables.library_exports, tables.export_owners, tables.function_imports, tables.variable_imports, tables.variable_stubs, tables.variable_exports };
}

bool KernelState::init(MemState &mem, const CallImportFunc &call_import, bool cpu_opt, bool thread_scheduler) {
    primary_address_space = &mem;
    corenum_allocator.set_max_core_count(MAX_CORE_COUNT);
    sched.set_enabled(thread_scheduler);
    start_tick = rtc_get_ticks(rtc_base_ticks());
    base_tick = { rtc_base_ticks() };
    this->call_import = call_import;
    this->cpu_opt = cpu_opt;

    return get_thread_return_code(mem) != 0;
}

void KernelState::load_process_param(MemState &mem, Ptr<uint32_t> ptr) {
    const SceProcessParam *param = ptr.cast<SceProcessParam>().get(mem);
    if (param->version == 0) {
        // Homebrews built with old vitasdk
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(process_param_mutex);
        process_params.insert_or_assign(&mem, ptr.cast<SceProcessParam>());
        const auto bindings = module_bindings(mem);
        const auto previous = bindings.exports.find(0xDF084DFA);
        auto [state, inserted] = process_libc_exports.try_emplace(&mem);
        if (inserted)
            state->second.previous = previous == bindings.exports.end()
                ? std::nullopt
                : std::optional<Address>(previous->second);
        state->second.published = param->sce_libc_param.address();
    }
    module_bindings(mem).exports[0xDF084DFA] = param->sce_libc_param.address();
}

void KernelState::unload_process_param(MemState &mem, Ptr<uint32_t> ptr) {
    const std::lock_guard<std::mutex> lock(process_param_mutex);
    const auto process = process_params.find(&mem);
    if (process == process_params.end() || process->second.address() != ptr.address())
        return;
    const auto state = process_libc_exports.find(&mem);
    auto bindings = module_bindings(mem);
    const auto current = bindings.exports.find(0xDF084DFA);
    if (state != process_libc_exports.end() && current != bindings.exports.end() && current->second == state->second.published) {
        if (state->second.previous)
            current->second = *state->second.previous;
        else
            bindings.exports.erase(current);
    }
    process_params.erase(process);
    process_libc_exports.erase(&mem);
}

Ptr<SceProcessParam> KernelState::get_process_param(MemState &mem) {
    const std::lock_guard<std::mutex> lock(process_param_mutex);
    const auto it = process_params.find(&mem);
    return it == process_params.end() ? Ptr<SceProcessParam>{} : it->second;
}

bool KernelState::register_process_identity(MemState &mem, ProcessIdentity identity) {
    if (identity.app_path.empty()) {
        LOG_ERROR("Cannot register a process with an empty application path");
        return false;
    }
    const std::lock_guard<std::mutex> lock(mutex);
    auto &state = thread_address_spaces[&mem];
    if (process_exiting || state.stopping || state.identity || state.creating != 0) {
        LOG_ERROR("Cannot register process identity '{}' in an occupied or stopping address space", identity.app_path);
        return false;
    }
    for (const auto &[_, thread] : threads) {
        if (&thread->cpu->mem == &mem) {
            LOG_ERROR("Cannot register process identity '{}' after threads have been created", identity.app_path);
            return false;
        }
    }
    if (identity.pid != 0 || next_process_id <= 0 || next_process_id == std::numeric_limits<SceUID>::max()) {
        LOG_ERROR("Cannot allocate process ID for '{}'", identity.app_path);
        return false;
    }
    identity.pid = next_process_id++;
    LOG_INFO("Registered process {} for '{}'", identity.pid, identity.app_path);
    state.start_tick = &mem == primary_address_space ? start_tick : rtc_get_ticks(base_tick.tick);
    state.identity = std::move(identity);
    return true;
}

std::optional<ProcessIdentity> KernelState::get_process_identity(MemState &mem) {
    const std::lock_guard<std::mutex> lock(mutex);
    const auto it = thread_address_spaces.find(&mem);
    return it == thread_address_spaces.end() ? std::nullopt : it->second.identity;
}

std::optional<uint64_t> KernelState::process_elapsed_time_at(MemState &mem, uint64_t clock_tick) {
    const std::lock_guard<std::mutex> lock(mutex);
    uint64_t origin = start_tick;
    if (&mem != primary_address_space) {
        const auto it = thread_address_spaces.find(&mem);
        if (it == thread_address_spaces.end() || !it->second.identity) {
            LOG_ERROR("Cannot get process time for an unregistered address space");
            return std::nullopt;
        }
        origin = it->second.start_tick;
    }
    if (clock_tick < origin) {
        LOG_ERROR("Cannot get process time before process creation");
        return std::nullopt;
    }
    return clock_tick - origin;
}

bool KernelState::set_libc_dso_handle_main(MemState &mem, Ptr<void> handle) {
    const std::lock_guard<std::mutex> lock(mutex);
    const auto it = thread_address_spaces.find(&mem);
    if (it == thread_address_spaces.end() || !it->second.identity || it->second.stopping) {
        LOG_ERROR("Cannot set libc main DSO handle for an unregistered or stopping process");
        return false;
    }
    it->second.libc_dso_handle_main = handle;
    return true;
}

std::optional<Ptr<void>> KernelState::get_libc_dso_handle_main(MemState &mem) {
    const std::lock_guard<std::mutex> lock(mutex);
    const auto it = thread_address_spaces.find(&mem);
    if (it == thread_address_spaces.end() || !it->second.identity)
        return std::nullopt;
    return it->second.libc_dso_handle_main;
}

bool KernelState::register_libc_exit_handler(MemState &mem, LibcExitHandler handler) {
    const std::lock_guard<std::mutex> lock(mutex);
    const auto it = thread_address_spaces.find(&mem);
    if (it == thread_address_spaces.end() || !it->second.identity || it->second.stopping || !handler.function) {
        LOG_ERROR("Cannot register libc exit handler for an unregistered or stopping process, or with a null function");
        return false;
    }
    it->second.libc_exit_handlers.push_back(handler);
    if (!it->second.libc_exit_warning_logged) {
        LOG_WARN("Libc exit handlers are registered for process {}, but automatic process finalization is not implemented", it->second.identity->pid);
        it->second.libc_exit_warning_logged = true;
    }
    return true;
}

std::optional<LibcExitHandler> KernelState::take_libc_exit_handler(MemState &mem, Ptr<void> dso) {
    const std::lock_guard<std::mutex> lock(mutex);
    const auto it = thread_address_spaces.find(&mem);
    if (it == thread_address_spaces.end() || !it->second.identity)
        return std::nullopt;
    auto &handlers = it->second.libc_exit_handlers;
    for (size_t i = handlers.size(); i > 0; --i) {
        if (!dso || handlers[i - 1].dso.address() == dso.address()) {
            const auto handler = handlers[i - 1];
            handlers.erase(handlers.begin() + (i - 1));
            return handler;
        }
    }
    return std::nullopt;
}

void KernelState::set_thread_tls_info(MemState &mem, ThreadTlsInfo info) {
    const std::lock_guard<std::mutex> lock(mutex);
    thread_tls_by_memory.insert_or_assign(&mem, info);
}

ThreadTlsInfo KernelState::get_thread_tls_info(MemState &mem) {
    const std::lock_guard<std::mutex> lock(mutex);
    const auto it = thread_tls_by_memory.find(&mem);
    return it == thread_tls_by_memory.end() ? ThreadTlsInfo{} : it->second;
}

void KernelState::clear_thread_tls_info(MemState &mem) {
    const std::lock_guard<std::mutex> lock(mutex);
    thread_tls_by_memory.erase(&mem);
}

Address KernelState::get_thread_return_code(MemState &mem) {
    const std::lock_guard<std::mutex> lock(mutex);
    const auto existing = thread_return_code_by_memory.find(&mem);
    if (existing != thread_return_code_by_memory.end())
        return existing->second.get();

    const Address address = alloc(mem, 4, "halt_instruction");
    if (!address) {
        LOG_ERROR("Failed to allocate thread return code in guest memory");
        return 0;
    }
    auto entry = thread_return_code_by_memory.try_emplace(&mem).first;
    entry->second = Block(address, [&mem](Address addr) { free(mem, addr); });
    const auto code = entry->second.get_ptr<uint16_t>().get(mem);
    code[0] = 0xBF00; // NOP
    code[1] = 0xBF30; // WFI
    return address;
}

bool KernelState::release_thread_address_space(MemState &mem) {
    std::unique_lock<std::mutex> lock(mutex);
    const auto state = thread_address_spaces.find(&mem);
    if (state == thread_address_spaces.end() || !state->second.stopping || state->second.creating != 0) {
        LOG_ERROR("Cannot release a thread address space before it is stopped");
        return false;
    }
    for (const auto &[_, thread] : threads) {
        if (&thread->cpu->mem == &mem) {
            LOG_ERROR("Cannot release thread resources while threads still belong to the address space");
            return false;
        }
    }
    for (const auto &[_, module] : loaded_modules) {
        if (module->address_space == &mem) {
            LOG_ERROR("Cannot release address space while module {} is still loaded", module->info.modid);
            return false;
        }
    }
    objects.remove_if<LwCond>([&](const LwCond &condition) { return condition.address_space == &mem; }, 0);
    objects.remove_if<LwMutex>([&](const LwMutex &mutex) { return mutex.address_space == &mem; }, 0);
    objects.remove_if<HeavyMutex>([&](const HeavyMutex &mutex) { return mutex.address_space == &mem; }, 0);
    objects.remove_if<Semaphore>([&](const Semaphore &semaphore) { return semaphore.address_space == &mem; }, 0);
    thread_return_code_by_memory.erase(&mem);
    ipmi_objects_by_memory.erase(&mem);
    thread_tls_by_memory.erase(&mem);
    loaded_sysmodules.erase(&mem);
    loaded_internal_sysmodules.erase(&mem);
    {
        const std::lock_guard<std::mutex> param_lock(process_param_mutex);
        process_params.erase(&mem);
        process_libc_exports.erase(&mem);
    }
    if (!state->second.libc_exit_handlers.empty())
        LOG_WARN("Discarding {} libc exit handlers during process teardown; automatic process finalization is not implemented", state->second.libc_exit_handlers.size());
    thread_address_spaces.erase(state);
    lock.unlock();
    {
        const std::lock_guard<std::mutex> export_lock(export_nids_mutex);
        module_uid_by_nid.erase(&mem);
        module_uid_layers.erase(&mem);
        secondary_module_bindings.erase(&mem);
    }
    return true;
}

void KernelState::set_memory_watch(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto &thread : threads) {
        auto &cpu = *thread.second->cpu;
        if (enabled != cpu.get_log_mem()) {
            if (enabled)
                cpu.set_log_mem(true);
            else
                cpu.set_log_mem(false);
        }
    }
}

void KernelState::invalidate_jit_cache(MemState &mem, Address start, size_t length) {
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto &[_, thread] : threads) {
        if (&thread->cpu->mem == &mem)
            thread->cpu->invalidate_jit_cache(start, length);
    }
}

uint64_t KernelState::process_time_at(uint64_t clock_time) const {
    // The process started at the host clock time start_tick - base_tick.tick
    return clock_time - (start_tick - base_tick.tick);
}

ThreadStatePtr KernelState::get_thread(SceUID thread_id) {
    const ThreadStatePtr thread = lock_and_find(thread_id, threads, mutex);
    // A deleted thread stays in threads until its host thread ends, but guest calls can't find it
    if (thread && thread->get_status() == ThreadStatus::deleted)
        return nullptr;
    return thread;
}

std::optional<SceUID> KernelState::get_thread_process_id(SceUID thread_id) {
    const ThreadStatePtr thread = get_thread(thread_id);
    if (!thread) {
        LOG_ERROR("Cannot get process ID for unknown thread {}", thread_id);
        return std::nullopt;
    }
    const auto identity = get_process_identity(thread->cpu->mem);
    if (!identity) {
        LOG_ERROR("Cannot get process ID for thread {}: no process identity", thread_id);
        return std::nullopt;
    }
    return identity->pid;
}

SceInt32 KernelState::check_primary_address_space_access(SceUID thread_id, const char *operation) {
    const ThreadStatePtr caller = get_thread(thread_id);
    if (!caller) {
        LOG_ERROR("Cannot use {} from unknown thread {}", operation, thread_id);
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }
    if (&caller->cpu->mem != primary_address_space) {
        LOG_ERROR("{} for a secondary address space is not implemented", operation);
        return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
    }
    return SCE_KERNEL_OK;
}

SceInt32 KernelState::check_primary_process_target(SceUID thread_id, SceUID target_pid, const char *operation) {
    if (const SceInt32 error = check_primary_address_space_access(thread_id, operation))
        return error;
    const std::lock_guard<std::mutex> lock(mutex);
    for (const auto &[mem, state] : thread_address_spaces) {
        if (state.identity && state.identity->pid == target_pid && mem != primary_address_space) {
            LOG_ERROR("{} targeting secondary process {} is not implemented", operation, target_pid);
            return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
        }
    }
    return SCE_KERNEL_OK;
}

SceInt32 KernelState::check_primary_module_loader(SceUID thread_id) {
    return check_primary_address_space_access(thread_id, "Module loading");
}

ThreadStatePtr KernelState::create_process_main_thread(MemState &mem, const char *name, Ptr<const void> entry_point) {
    SceInt32 priority = SCE_KERNEL_DEFAULT_PRIORITY_USER;
    SceInt32 stack_size = SCE_KERNEL_STACK_SIZE_USER_MAIN;
    SceInt32 affinity = SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT;
    const auto *process_param = get_process_param(mem).get(mem);
    if (process_param) {
        const auto priority_ptr = Ptr<int32_t>(process_param->main_thread_priority);
        if (priority_ptr)
            priority = *priority_ptr.get(mem);
        const auto stack_size_ptr = Ptr<int32_t>(process_param->main_thread_stacksize);
        if (stack_size_ptr)
            stack_size = *stack_size_ptr.get(mem);
        const auto affinity_ptr = Ptr<SceInt32>(process_param->main_thread_cpu_affinity_mask);
        if (affinity_ptr)
            affinity = *affinity_ptr.get(mem);
    }
    if (affinity == SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT)
        affinity = SCE_KERNEL_CPU_MASK_USER_ALL;
    return create_thread(mem, name, entry_point, priority, affinity, stack_size, 0);
}

SceInt32 KernelState::start_process_main_thread(const ThreadStatePtr &thread, std::span<const std::string> args) {
    if (!thread) {
        LOG_ERROR("Cannot start a process without its main thread");
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }
    size_t size = 0;
    for (const auto &arg : args) {
        if (arg.size() >= static_cast<size_t>(thread->stack_size) || size >= static_cast<size_t>(thread->stack_size) - arg.size()) {
            LOG_ERROR("Process arguments exceed the main thread stack");
            return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
        }
        size += arg.size() + 1;
    }
    if (size && (size + 7) / 8 * 8 >= static_cast<size_t>(thread->stack_size)) {
        LOG_ERROR("Process arguments leave no main thread stack space");
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
    }
    if (!size)
        return thread->start(0, Ptr<void>{}, true);

    MemState &mem = thread->cpu->mem;
    const Address address = alloc(mem, static_cast<uint32_t>(size), "arg");
    if (!address) {
        LOG_ERROR("Failed to allocate process arguments");
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
    Block allocation(address, [&mem](Address addr) { free(mem, addr); });
    auto *output = allocation.get_ptr<char>().get(mem);
    for (const auto &arg : args) {
        std::memcpy(output, arg.c_str(), arg.size() + 1);
        output += arg.size() + 1;
    }
    return thread->start(static_cast<SceSize>(size), Ptr<void>(address), true);
}

ProcessStartResult KernelState::start_process(MemState &mem, const char *name, SceUID main_module_id, std::span<const std::string> args,
    const std::function<void(const ThreadStatePtr &)> &on_main_created,
    const std::function<void(const KernelModule &)> &start_module) {
    const auto modules = get_process_start_modules(mem, main_module_id);
    if (!modules)
        return ProcessStartResult::MainModuleUnavailable;
    const auto main_thread = create_process_main_thread(mem, name, modules->main.module()->info.start_entry);
    if (!main_thread) {
        LOG_ERROR("Failed to create main thread for process {}", name);
        return ProcessStartResult::MainThreadCreationFailed;
    }
    on_main_created(main_thread);
    for (const auto &module : modules->preloaded) {
        const auto lease = acquire_module_start(mem, module);
        if (!lease) {
            LOG_WARN("Skipping preloaded module {} because it is no longer loaded", module->info.modid);
            continue;
        }
        start_module(*module);
    }
    const SceInt32 result = start_process_main_thread(main_thread, args);
    if (result < 0) {
        LOG_ERROR("Failed to start main thread for process {}: {}", name, log_hex(result));
        main_thread->remove();
        return ProcessStartResult::MainThreadStartFailed;
    }
    return ProcessStartResult::Success;
}

SceInt32 KernelState::run_module_entry(const KernelModule &module, bool start, SceSize args, Ptr<const void> argp) {
    const auto entry = start ? module.info.start_entry : module.info.stop_entry;
    if (!entry)
        return SCE_KERNEL_OK;

    const auto thread = create_thread(*module.address_space, module.info.module_name, entry,
        SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT, SCE_KERNEL_STACK_SIZE_USER_MAIN, 0);
    if (!thread) {
        LOG_ERROR("Failed to create {} thread for module {}", start ? "start" : "stop", module.info.module_name);
        return SCE_KERNEL_ERROR_ERROR;
    }
    const SceInt32 result = static_cast<SceInt32>(thread->start_and_wait(args, argp.cast<void>()));
    if (start && result != SCE_KERNEL_OK && !is_primary_address_space(*module.address_space)) {
        objects.remove_if<LwCond>([&](const LwCond &condition) {
            return condition.address_space == module.address_space && condition.creator_thread == thread->id;
        },
            thread->id);
        objects.remove_if<LwMutex>([&](const LwMutex &mutex) {
            return mutex.address_space == module.address_space && mutex.creator_thread == thread->id;
        },
            thread->id);
        objects.remove_if<HeavyMutex>([&](const HeavyMutex &mutex) {
            return mutex.address_space == module.address_space && mutex.creator_thread == thread->id;
        },
            thread->id);
        objects.remove_if<Semaphore>([&](const Semaphore &semaphore) {
            return semaphore.address_space == module.address_space && semaphore.creator_thread == thread->id;
        },
            thread->id);
    }
    thread->remove();
    return result;
}

ThreadStatePtr KernelState::create_thread_from_caller(const ThreadStatePtr &caller, const char *name, Ptr<const void> entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, SceUInt32 attr) {
    if (!caller) {
        LOG_ERROR("Cannot create a thread without a caller");
        return nullptr;
    }
    return create_thread(caller->cpu->mem, name, entry_point, init_priority, affinity_mask, stack_size, attr);
}

ThreadStatePtr KernelState::create_thread(MemState &mem, const char *name, Ptr<const void> entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, SceUInt32 attr) {
    ThreadAddressSpaceState *space_state;
    {
        const std::lock_guard<std::mutex> lock(mutex);
        auto &state = thread_address_spaces[&mem];
        if (process_exiting || state.stopping) {
            LOG_ERROR("Cannot create thread '{}' in a stopping address space", name);
            return nullptr;
        }
        ++state.creating;
        space_state = &state;
    }
    const auto finish_creation = [&] {
        const std::lock_guard<std::mutex> lock(mutex);
        --space_state->creating;
        thread_deleted_cond.notify_all();
    };
    const Address return_code = get_thread_return_code(mem);
    if (!return_code) {
        finish_creation();
        return nullptr;
    }
    const SceUID id = get_next_uid();
    const int allocated_core = corenum_allocator.new_corenum();
    int core_num = allocated_core;
    if (core_num < 0) {
        LOG_ERROR("Out of core number to allocate, use 0");
        core_num = 0;
    }
    CPUStatePtr cpu = init_cpu(cpu_opt, id, static_cast<std::size_t>(core_num), mem);
    if (!cpu) {
        if (allocated_core >= 0)
            corenum_allocator.free_corenum(allocated_core);
        finish_creation();
        return nullptr;
    }
    // A guest function the kernel calls returns to the halt instruction, in Thumb mode
    cpu->return_pc = return_code | 1;
    cpu->set_log_code(debugger.watch_code);
    cpu->set_log_mem(debugger.watch_memory);
    ThreadStatePtr thread = std::make_shared<ThreadState>(id, *this, mem, std::move(cpu), name, entry_point.address(), init_priority, affinity_mask, stack_size, attr);

    {
        const std::lock_guard<std::mutex> lock(mutex);
        threads.emplace(thread->id, thread);
        // A new thread starts with the debugger's hold every thread has, and sees the pause too
        thread->set_suspended(threads_suspended);
        if (process_exiting || space_state->stopping)
            thread->remove();
    }

    ThreadParams params;
    params.kernel = this;
    params.thread = thread;

    params.host_may_destroy_params = SDL_CreateSemaphore(0);
    const auto host_start_failed = [&] {
        {
            const std::lock_guard<std::mutex> lock(mutex);
            thread->remove();
            threads.erase(id);
        }
        if (allocated_core >= 0)
            corenum_allocator.free_corenum(allocated_core);
        finish_creation();
    };
    if (!params.host_may_destroy_params) {
        LOG_ERROR("Failed to create thread startup semaphore: {}", SDL_GetError());
        host_start_failed();
        return nullptr;
    }
    SDL_Thread *host_thread = SDL_CreateThread(&thread_function, thread->name, &params);
    if (!host_thread) {
        LOG_ERROR("Failed to create host thread: {}", SDL_GetError());
        SDL_DestroySemaphore(params.host_may_destroy_params);
        host_start_failed();
        return nullptr;
    }
    SDL_DetachThread(host_thread);
    SDL_WaitSemaphore(params.host_may_destroy_params);
    SDL_DestroySemaphore(params.host_may_destroy_params);

    finish_creation();
    return thread;
}

static Ptr<Ptr<void>> thread_tls_addr(const ThreadStatePtr &thread, SceUID thread_id, int key) {
    if (!thread) {
        LOG_ERROR("Cannot get TLS address for unknown thread {}", thread_id);
        return {};
    }
    // Magic numbers taken from decompiled source. There is 0x400 unused bytes of unknown usage.
    if (key < 0 || key > 0x100) {
        LOG_ERROR("Wrong tls slot index. TID:{} index:{}", thread_id, key);
        return {};
    }
    return thread->tls.get_ptr<Ptr<void>>() + key;
}

Ptr<Ptr<void>> KernelState::get_thread_tls_addr(SceUID thread_id, int key) {
    return thread_tls_addr(get_thread(thread_id), thread_id, key);
}

Ptr<Ptr<void>> KernelState::get_thread_tls_addr(MemState &mem, SceUID thread_id, int key) {
    const ThreadStatePtr thread = get_thread(thread_id);
    if (thread && &thread->cpu->mem != &mem) {
        LOG_ERROR("Cannot get TLS address for thread {} from another address space", thread_id);
        return {};
    }
    return thread_tls_addr(thread, thread_id, key);
}

SceInt32 KernelState::request_process_exit(SceUID caller_id, int res, std::optional<AppLaunchRequest> relaunch) {
    if (const SceInt32 error = check_primary_address_space_access(caller_id, "Process exit"))
        return error;
    if (!process_exit_callback) {
        LOG_ERROR("Cannot request process exit without a session callback");
        return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
    }
    process_exit_callback(res, std::move(relaunch));
    return SCE_KERNEL_OK;
}

void KernelState::stop_threads_in_address_space(MemState &mem) {
    std::unique_lock<std::mutex> lock(mutex);
    auto &state = thread_address_spaces[&mem];
    state.stopping = true;
    thread_deleted_cond.wait(lock, [&] { return state.creating == 0; });
    for (auto &[_, thread] : threads) {
        if (&thread->cpu->mem == &mem)
            thread->remove();
    }
    thread_deleted_cond.wait(lock, [&] {
        for (const auto &[_, thread] : threads) {
            if (&thread->cpu->mem == &mem)
                return false;
        }
        return true;
    });
}

void KernelState::process_exit() {
    std::unique_lock<std::mutex> lock(mutex);
    process_exiting = true;
    thread_deleted_cond.wait(lock, [this] {
        for (const auto &[_, state] : thread_address_spaces) {
            if (state.creating != 0)
                return false;
        }
        return true;
    });
    for (auto &[_, thread] : threads)
        thread->remove();
    thread_deleted_cond.wait(lock, [this] { return threads.empty(); });
}

void KernelState::set_threads_paused(bool paused) {
    const std::lock_guard<std::mutex> lock(mutex);
    if (threads_paused == paused)
        return;
    // Set before the threads see it, so a thread that checks its holds meanwhile already sees it
    threads_paused = paused;
    for (auto &[_, thread] : threads)
        thread->pause_changed();
}

void KernelState::set_threads_suspended(bool suspended) {
    const std::lock_guard<std::mutex> lock(mutex);
    threads_suspended = suspended;
    for (auto &[_, thread] : threads)
        thread->set_suspended(suspended);
}

void KernelState::deinit(MemState &mem) {
    process_exit();
    threads.clear();

    objects.clear();

    loaded_modules.clear();
    loaded_sysmodules.clear();
    loaded_internal_sysmodules.clear();

    {
        std::lock_guard<std::mutex> lock(export_nids_mutex);
        export_nids.clear();
        export_nids_by_lib.clear();
        export_nid_owners.clear();
        func_binding_infos.clear();
        var_binding_infos.clear();
        variable_stubs.clear();
        variable_exports.clear();
        module_uid_by_nid.clear();
        module_uid_layers.clear();
        secondary_module_bindings.clear();
    }

    corenum_allocator.alloc.reset();
    corenum_allocator.alloc.set_maximum(0);
    sched.reset();

    thread_tls_by_memory.clear();

    thread_event_start = Ptr<const void>(0);
    thread_event_start_arg = 0;
    thread_event_end = Ptr<const void>(0);
    thread_event_end_arg = 0;

    codec_blocks.clear();

    thread_return_code_by_memory.clear();
    ipmi_objects_by_memory.clear();
    for (const auto &[_, state] : thread_address_spaces) {
        if (!state.libc_exit_handlers.empty())
            LOG_WARN("Discarding {} libc exit handlers during session shutdown; automatic process finalization is not implemented", state.libc_exit_handlers.size());
    }
    thread_address_spaces.clear();
    obj_store.clear();

    {
        const std::lock_guard<std::mutex> lock(process_param_mutex);
        process_params.clear();
        process_libc_exports.clear();
    }
    primary_address_space = nullptr;
    debugger.deinit();

    next_uid = 1;
    next_process_id = 1;

    threads_paused = false;
    process_exiting = false;
}

SceKernelModulePtr KernelState::find_module(const MemState &mem, SceUID module_id) {
    const auto lock = std::lock_guard(mutex);
    const auto it = loaded_modules.find(module_id);
    return it != loaded_modules.end() && it->second->address_space == &mem ? it->second : nullptr;
}

std::optional<ProcessStartModules> KernelState::get_process_start_modules(const MemState &mem, SceUID main_module_id) {
    const std::lock_guard<std::mutex> linker_lock(module_linker_mutex);
    const std::lock_guard<std::mutex> lock(mutex);
    const auto main = loaded_modules.find(main_module_id);
    if (main == loaded_modules.end() || main->second->address_space != &mem) {
        LOG_ERROR("Cannot start process: main module {} is not loaded in its address space", main_module_id);
        return std::nullopt;
    }
    ProcessStartModules result{ ModuleStartLease(main->second), {} };
    for (const auto &[uid, module] : loaded_modules) {
        if (uid != main_module_id && module->address_space == &mem)
            result.preloaded.push_back(module);
    }
    return result;
}

std::optional<ModuleStartLease> KernelState::acquire_module_start(const MemState &mem, const SceKernelModulePtr &module) {
    if (!module) {
        LOG_ERROR("Cannot start a null module");
        return std::nullopt;
    }
    const std::lock_guard<std::mutex> linker_lock(module_linker_mutex);
    const std::lock_guard<std::mutex> lock(mutex);
    const auto it = loaded_modules.find(module->info.modid);
    if (it == loaded_modules.end() || it->second != module || module->address_space != &mem)
        return std::nullopt;
    return ModuleStartLease(module);
}

SceKernelModulePtr KernelState::find_module_by_path(const MemState &mem, const std::string &path) {
    const auto lock = std::lock_guard(mutex);
    for (const auto &[_, module] : loaded_modules) {
        if (module->address_space == &mem && path == module->info.path)
            return module;
    }
    return nullptr;
}

SceKernelModulePtr KernelState::find_module_by_addr(const MemState &mem, Address address) {
    const auto lock = std::lock_guard(mutex);
    for (auto &[_, mod] : loaded_modules) {
        if (mod->address_space != &mem)
            continue;
        for (auto &seg : mod->info.segments) {
            if (!seg.size)
                continue;
            if (seg.vaddr.address() <= address && address <= seg.vaddr.address() + seg.memsz) {
                return mod;
            }
        }
    }
    return nullptr;
}
