// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include "SceFiber.h"

#include "cpu/functions.h"

#include <sstream>
#include <util/lock_and_find.h>
#include <util/log.h>

const static int DEFAULT_FIBER_STACK_SIZE = 4096;

struct FiberState {
    std::mutex mutex;
    std::map<SceUID, SceFiber *> thread_fibers;
    std::map<SceUID, CPUContext> thread_contexts;
};

LIBRARY_INIT_IMPL(SceFiber) {
    host.kernel.obj_store.create<FiberState>();
}
LIBRARY_INIT_REGISTER(SceFiber)

constexpr bool LOG_FIBER = false;

void set_thread_fiber(FiberState &state, const SceUID &tid, SceFiber *fiber) {
    const std::lock_guard<std::mutex> lock(state.mutex);
    state.thread_fibers[tid] = fiber;
}

SceFiber *get_thread_fiber(FiberState &state, const SceUID &tid) {
    const std::lock_guard<std::mutex> lock(state.mutex);
    if (state.thread_fibers.find(tid) == state.thread_fibers.end()) {
        return nullptr;
    }
    return state.thread_fibers[tid];
}

void set_thread_context(FiberState &state, const SceUID &tid, const CPUContext &ctx) {
    const std::lock_guard<std::mutex> lock(state.mutex);
    state.thread_contexts[tid] = ctx;
}

CPUContext get_thread_context(FiberState &state, const SceUID &tid) {
    const std::lock_guard<std::mutex> lock(state.mutex);
    return state.thread_contexts[tid];
}

std::string describe_fiber(FiberState &state, ThreadStatePtr thread, SceFiber *fiber) {
    std::stringstream ss;
    ss << fmt::format("Fiber (name: {})\n", fiber->name);
    ss << fmt::format("entry: {}\n", log_hex(fiber->cpu->get_pc()), log_hex(fiber->entry.address()));
    ss << "CPU Context:\n";
    ss << fiber->cpu->description();
    ss << "Referenced from " << thread->id << "\n";
    ss << "CPU Context:\n";
    auto ctx = get_thread_context(state, thread->id);
    ss << ctx.description();
    return ss.str();
}

void log_fiber(FiberState &state, ThreadStatePtr thread, SceFiber *fiber, const std::string &function_name) {
    std::string log_msg = function_name + "\n";
    log_msg += describe_fiber(state, thread, fiber);
    LOG_INFO("{}", log_msg);
}

void setup_fiber_to_run(HostState &host, const ThreadStatePtr thread, SceFiber *fiber, const uint32_t &argOnRunTo) {
    assert(fiber->status != FiberStatus::RUN);
    if (!fiber->addrContext) {
        fiber->cpu->set_sp(read_sp(*thread->cpu));
        fiber->status = FiberStatus::INIT;
    }

    if (fiber->status == FiberStatus::INIT) {
        fiber->cpu->cpu_registers[0] = fiber->argOnInitialize;
        fiber->cpu->cpu_registers[1] = argOnRunTo;
        fiber->cpu->set_pc(fiber->entry.address());
    } else {
        if (fiber->argOnRun) {
            *fiber->argOnRun.get(host.mem) = argOnRunTo;
        }
    }
    fiber->status = FiberStatus::RUN;
}

void initialize_fiber(HostState &host, const ThreadStatePtr thread, SceFiber *fiber, const char *name, Ptr<SceFiberEntry> entry, SceUInt32 argOnInitialize, Ptr<void> addrContext, SceSize sizeContext, SceFiberOptParam *params) {
    const auto state = host.kernel.obj_store.get<FiberState>();
    fiber->entry = entry;
    strncpy(fiber->name, name, 32);
    fiber->argOnInitialize = argOnInitialize;
    fiber->argOnRun = 0;
    fiber->addrContext = addrContext.address();
    fiber->sizeContext = sizeContext;
    fiber->cpu = new CPUContext;
    fiber->status = FiberStatus::INIT;
    *fiber->cpu = save_context(*thread->cpu);

    if (addrContext.address() && sizeContext != 0) {
        memset(addrContext.get(host.mem), 0xCC, sizeContext);
        fiber->cpu->set_sp(addrContext.address() + sizeContext);
    }
    fiber->cpu->set_lr(0xDEADBEAF);
}

EXPORT(int, _sceFiberAttachContextAndRun, SceFiber *fiber, Address addrContext, SceSize sizeContext, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
    // Maybe Need more check on real hw
    STUBBED("Todo: not sure for now");
    const auto state = host.kernel.obj_store.get<FiberState>();
    const auto thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    SceFiber *thread_fiber = get_thread_fiber(*state, thread->id);
    assert(!thread_fiber);
    if (LOG_FIBER) {
        log_fiber(*state, thread, fiber, "Attach context and run");
    }

    fiber->addrContext = addrContext;
    fiber->sizeContext = sizeContext;
    if (!addrContext && sizeContext > 0) {
        fiber->cpu->set_sp(addrContext + sizeContext);
    }

    setup_fiber_to_run(host, thread, fiber, argOnRunTo);
    set_thread_context(*state, thread->id, save_context(*thread->cpu));
    set_thread_fiber(*state, thread->id, fiber);

    load_context(*thread->cpu, *fiber->cpu);
    return fiber->cpu->cpu_registers[0];
}

EXPORT(int, _sceFiberAttachContextAndSwitch, SceFiber *fiber, Address addrContext, SceSize sizeContext, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
    // Maybe Need more check on real hw
    STUBBED("Todo: not sure for now");
    const auto state = host.kernel.obj_store.get<FiberState>();
    const auto thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    SceFiber *thread_fiber = get_thread_fiber(*state, thread->id);
    if (LOG_FIBER) {
        log_fiber(*state, thread, fiber, "Attach context and switch");
    }

    fiber->addrContext = addrContext;
    fiber->sizeContext = sizeContext;
    if (!addrContext && sizeContext > 0) {
        fiber->cpu->set_sp(addrContext + sizeContext);
    }

    *thread_fiber->cpu = save_context(*thread->cpu);
    setup_fiber_to_run(host, thread, fiber, argOnRunTo);
    thread_fiber->status = FiberStatus::SUSPEND;
    thread_fiber->argOnRun = argOnRun;
    thread_fiber->cpu->cpu_registers[0] = SCE_FIBER_OK;
    set_thread_fiber(*state, thread->id, fiber);
    load_context(*thread->cpu, *fiber->cpu);

    return fiber->cpu->cpu_registers[0];
}

EXPORT(SceInt32, _sceFiberInitializeImpl, SceFiber *fiber, const char *name, Ptr<SceFiberEntry> entry, SceUInt32 argOnInitialize, Ptr<void> addrContext, SceSize sizeContext, SceFiberOptParam *params) {
    if (!fiber || !entry || !name) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    if ((sizeContext != 0) && (sizeContext < SCE_FIBER_CONTEXT_MINIMUM_SIZE)) {
        return RET_ERROR(SCE_FIBER_ERROR_RANGE);
    }

    if ((!addrContext && (sizeContext != 0)) || (addrContext && (sizeContext == 0)) || ((sizeContext & 7) != 0)) {
        return RET_ERROR(SCE_FIBER_ERROR_INVALID);
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    if (!thread) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }

    initialize_fiber(host, thread, fiber, name, entry, argOnInitialize, addrContext, sizeContext, params);

    return SCE_FIBER_OK;
}

EXPORT(int, _sceFiberInitializeWithInternalOptionImpl, SceFiber *fiber, const char *name, Ptr<SceFiberEntry> entry, SceUInt32 argOnInitialize, Ptr<void> addrContext, SceSize sizeContext) {
    if (!fiber || !entry || !name) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    if ((sizeContext != 0) && (sizeContext < SCE_FIBER_CONTEXT_MINIMUM_SIZE)) {
        return RET_ERROR(SCE_FIBER_ERROR_RANGE);
    }

    if ((!addrContext && (sizeContext != 0)) || (addrContext && (sizeContext == 0)) || ((sizeContext & 7) != 0)) {
        return RET_ERROR(SCE_FIBER_ERROR_INVALID);
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    if (!thread) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }

    initialize_fiber(host, thread, fiber, name, entry, argOnInitialize, addrContext, sizeContext, nullptr);

    return SCE_FIBER_OK;
}

EXPORT(SceInt32, sceFiberFinalize, SceFiber *fiber) {
    if (!fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    delete fiber->cpu;
    return 0;
}

EXPORT(int, sceFiberGetInfo) {
    return UNIMPLEMENTED();
}

EXPORT(SceUInt32, sceFiberGetSelf, Ptr<SceFiber> *fiber) {
    const auto state = host.kernel.obj_store.get<FiberState>();
    if (!fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    SceFiber *thread_fiber = get_thread_fiber(*state, thread->id);
    if (thread_fiber)
        *fiber = Ptr<SceFiber>(thread_fiber, host.mem);
    else
        *fiber = Ptr<SceFiber>(0);

    return SCE_FIBER_OK;
}

EXPORT(int, sceFiberOptParamInitialize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberPopUserMarkerWithHud) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberPushUserMarkerWithHud) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberRenameSelf) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceFiberReturnToThread, uint32_t argOnReturnTo, Ptr<uint32_t> argOnRun) {
    const auto state = host.kernel.obj_store.get<FiberState>();
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    SceFiber *fiber = get_thread_fiber(*state, thread->id);
    CPUContext thread_context = get_thread_context(*state, thread->id);
    assert(fiber->status == FiberStatus::RUN);
    if (LOG_FIBER) {
        log_fiber(*state, thread, fiber, "Return to thread");
    }

    *fiber->cpu = save_context(*thread->cpu);
    fiber->cpu->cpu_registers[0] = SCE_FIBER_OK;
    fiber->status = FiberStatus::SUSPEND;
    fiber->argOnRun = argOnRun;
    set_thread_fiber(*state, thread->id, nullptr);

    load_context(*thread->cpu, thread_context);
    Address argOnReturn = thread_context.cpu_registers[2];
    if (argOnReturn) {
        *(Ptr<uint32_t>(argOnReturn).get(host.mem)) = argOnReturnTo;
    }

    return SCE_FIBER_OK;
}

EXPORT(SceUInt32, sceFiberRun, SceFiber *fiber, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnReturn) {
    const auto state = host.kernel.obj_store.get<FiberState>();
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    if (!fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    SceFiber *thread_fiber = get_thread_fiber(*state, thread->id);
    assert(!thread_fiber);
    if (LOG_FIBER) {
        log_fiber(*state, thread, fiber, "Run");
    }

    setup_fiber_to_run(host, thread, fiber, argOnRunTo);
    set_thread_context(*state, thread->id, save_context(*thread->cpu));
    set_thread_fiber(*state, thread->id, fiber);

    load_context(*thread->cpu, *fiber->cpu);
    return fiber->cpu->cpu_registers[0];
}

EXPORT(int, sceFiberStartContextSizeCheck) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberStopContextSizeCheck) {
    return UNIMPLEMENTED();
}

EXPORT(SceUInt32, sceFiberSwitch, SceFiber *fiber, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
    const auto state = host.kernel.obj_store.get<FiberState>();
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    if (!fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    SceFiber *thread_fiber = get_thread_fiber(*state, thread->id);
    assert(thread_fiber);
    if (LOG_FIBER) {
        log_fiber(*state, thread, fiber, "Switch");
    }

    *thread_fiber->cpu = save_context(*thread->cpu);
    thread_fiber->status = FiberStatus::SUSPEND;
    thread_fiber->argOnRun = argOnRun;
    thread_fiber->cpu->cpu_registers[0] = SCE_FIBER_OK;
    set_thread_fiber(*state, thread->id, fiber);
    setup_fiber_to_run(host, thread, fiber, argOnRunTo);
    load_context(*thread->cpu, *fiber->cpu);

    return fiber->cpu->cpu_registers[0];
}

BRIDGE_IMPL(_sceFiberAttachContextAndRun)
BRIDGE_IMPL(_sceFiberAttachContextAndSwitch)
BRIDGE_IMPL(_sceFiberInitializeImpl)
BRIDGE_IMPL(_sceFiberInitializeWithInternalOptionImpl)
BRIDGE_IMPL(sceFiberFinalize)
BRIDGE_IMPL(sceFiberGetInfo)
BRIDGE_IMPL(sceFiberGetSelf)
BRIDGE_IMPL(sceFiberOptParamInitialize)
BRIDGE_IMPL(sceFiberPopUserMarkerWithHud)
BRIDGE_IMPL(sceFiberPushUserMarkerWithHud)
BRIDGE_IMPL(sceFiberRenameSelf)
BRIDGE_IMPL(sceFiberReturnToThread)
BRIDGE_IMPL(sceFiberRun)
BRIDGE_IMPL(sceFiberStartContextSizeCheck)
BRIDGE_IMPL(sceFiberStopContextSizeCheck)
BRIDGE_IMPL(sceFiberSwitch)
