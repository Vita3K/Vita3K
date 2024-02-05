// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <module/module.h>
#include <modules/module_parent.h>

#include <cpu/functions.h>
#include <kernel/state.h>

#include <sstream>
#include <util/lock_and_find.h>
#include <util/log.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceFiber);

#define SCE_FIBER_CONTEXT_MINIMUM_SIZE 512

enum SceFiberErrorCode : uint32_t {
    SCE_FIBER_OK = 0x00000000, //!< Success
    SCE_FIBER_ERROR_NULL = 0x80590001, //!< Some parameters are NULL.
    SCE_FIBER_ERROR_ALIGNMENT = 0x80590002, //!< Some pointer-parameters are not aligned in their proper alignments.
    SCE_FIBER_ERROR_RANGE = 0x80590003, //!< A parameter exceeds its range in the specification.
    SCE_FIBER_ERROR_INVALID = 0x80590004, //!< A parameter has an invalid value.
    SCE_FIBER_ERROR_PERMISSION = 0x80590005, //!< The function was called from the entity which does not have the permission.
    SCE_FIBER_ERROR_STATE = 0x80590006, //!< The function was applied to a fiber in the state which the function does not support.
    SCE_FIBER_ERROR_BUSY = 0x80590007, //!< The module specified by the function is busy.
    SCE_FIBER_ERROR_AGAIN = 0x80590008, //!< The function could not complete because of the situation. Please try again later.
    SCE_FIBER_ERROR_FATAL = 0x80590009, //!< The fiber caused an unrecoverable error.
};

typedef void(SceFiberEntry)(SceUInt32 argOnInitialize, SceUInt32 argOnRun);

struct SceFiberOptParam {
    char reserved[128];
};

enum class FiberStatus {
    INIT,
    SUSPEND,
    RUN
};

typedef struct SceFiber {
    Ptr<SceFiberEntry> entry;
    Address addrContext;
    SceSize sizeContext;
    char name[32];
    CPUContext *cpu;
    SceUInt32 argOnInitialize;
    Ptr<uint32_t> argOnRun;
    FiberStatus status;
} SceFiber;

static_assert(sizeof(SceFiber) <= 128, "SceFiber struct size is more than 128");

struct FiberState {
    std::mutex mutex;
    std::map<SceUID, SceFiber *> thread_fibers;
    std::map<SceUID, CPUContext> thread_contexts;
};

LIBRARY_INIT(SceFiber) {
    emuenv.kernel.obj_store.create<FiberState>();
}

constexpr bool LOG_FIBER = false;

void set_thread_fiber(FiberState &state, const SceUID &tid, SceFiber *fiber) {
    state.thread_fibers[tid] = fiber;
}

SceFiber *get_thread_fiber(FiberState &state, const SceUID &tid) {
    auto fiber = state.thread_fibers.find(tid);
    if (fiber == state.thread_fibers.end()) {
        return nullptr;
    }
    return fiber->second;
}

void set_thread_context(FiberState &state, const SceUID &tid, const CPUContext &ctx) {
    state.thread_contexts[tid] = ctx;
}

CPUContext get_thread_context(FiberState &state, const SceUID &tid) {
    return state.thread_contexts[tid];
}

std::string describe_fiber(FiberState &state, const ThreadStatePtr &thread, SceFiber *fiber) {
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

void log_fiber(FiberState &state, const ThreadStatePtr &thread, SceFiber *fiber, const std::string &function_name) {
    std::string log_msg = function_name + "\n";
    log_msg += describe_fiber(state, thread, fiber);
    LOG_INFO("{}", log_msg);
}

void setup_fiber_to_run(EmuEnvState &emuenv, const ThreadStatePtr &thread, SceFiber *fiber, uint32_t thread_sp, const uint32_t &argOnRunTo) {
    assert(fiber->status != FiberStatus::RUN);
    if (!fiber->addrContext) {
        fiber->cpu->set_sp(thread_sp);
        fiber->status = FiberStatus::INIT;
    }

    if (fiber->status == FiberStatus::INIT) {
        fiber->cpu->cpu_registers[0] = fiber->argOnInitialize;
        fiber->cpu->cpu_registers[1] = argOnRunTo;
        fiber->cpu->set_pc(fiber->entry.address());
    } else {
        if (fiber->argOnRun) {
            *fiber->argOnRun.get(emuenv.mem) = argOnRunTo;
        }
    }
    fiber->status = FiberStatus::RUN;
}

void initialize_fiber(EmuEnvState &emuenv, const ThreadStatePtr &thread, SceFiber *fiber, const char *name, Ptr<SceFiberEntry> entry, SceUInt32 argOnInitialize, Ptr<void> addrContext, SceSize sizeContext, SceFiberOptParam *params) {
    fiber->entry = entry;
    strncpy(fiber->name, name, 32);
    fiber->argOnInitialize = argOnInitialize;
    fiber->argOnRun = nullptr;
    fiber->addrContext = addrContext.address();
    fiber->sizeContext = sizeContext;
    fiber->cpu = new CPUContext;
    fiber->status = FiberStatus::INIT;
    *fiber->cpu = save_context(*thread->cpu);

    if (addrContext && sizeContext > 0) {
        memset(addrContext.get(emuenv.mem), 0xCC, sizeContext);
        fiber->cpu->set_sp(addrContext.address() + sizeContext);
    }
    fiber->cpu->set_lr(0xDEADBEAF);
}

EXPORT(int, _sceFiberAttachContextAndRun, SceFiber *fiber, Address addrContext, SceSize sizeContext, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
    TRACY_FUNC(_sceFiberAttachContextAndRun, fiber, addrContext, sizeContext, argOnRunTo, argOnRun);
    // Maybe Need more check on real hw
    STUBBED("Todo: not sure for now");
    const auto state = emuenv.kernel.obj_store.get<FiberState>();
    const std::lock_guard<std::mutex> lock(state->mutex);
    const auto thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    assert(!get_thread_fiber(*state, thread->id));
    assert(!fiber->addrContext);
    if (LOG_FIBER) {
        log_fiber(*state, thread, fiber, "Attach context and run");
    }

    fiber->addrContext = addrContext;
    fiber->sizeContext = sizeContext;
    if (addrContext && sizeContext > 0) {
        fiber->cpu->set_sp(addrContext + sizeContext);
    }

    setup_fiber_to_run(emuenv, thread, fiber, read_sp(*thread->cpu), argOnRunTo);
    set_thread_context(*state, thread->id, save_context(*thread->cpu));
    set_thread_fiber(*state, thread->id, fiber);

    load_context(*thread->cpu, *fiber->cpu);
    return fiber->cpu->cpu_registers[0];
}

EXPORT(int, _sceFiberAttachContextAndSwitch, SceFiber *fiber, Address addrContext, SceSize sizeContext, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
    TRACY_FUNC(_sceFiberAttachContextAndSwitch, fiber, addrContext, sizeContext, argOnRunTo, argOnRun);
    // Maybe Need more check on real hw
    STUBBED("Todo: not sure for now");
    const auto state = emuenv.kernel.obj_store.get<FiberState>();
    const std::lock_guard<std::mutex> lock(state->mutex);
    const auto thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    auto ctx = get_thread_context(*state, thread->id);
    SceFiber *thread_fiber = get_thread_fiber(*state, thread->id);
    if (LOG_FIBER) {
        log_fiber(*state, thread, fiber, "Attach context and switch");
    }

    assert(thread_fiber);
    assert(!fiber->addrContext);
    fiber->addrContext = addrContext;
    fiber->sizeContext = sizeContext;
    if (addrContext && sizeContext > 0) {
        fiber->cpu->set_sp(addrContext + sizeContext);
    }

    *thread_fiber->cpu = save_context(*thread->cpu);
    setup_fiber_to_run(emuenv, thread, fiber, ctx.get_sp(), argOnRunTo);
    thread_fiber->status = FiberStatus::SUSPEND;
    thread_fiber->argOnRun = argOnRun;
    thread_fiber->cpu->cpu_registers[0] = SCE_FIBER_OK;
    set_thread_fiber(*state, thread->id, fiber);
    load_context(*thread->cpu, *fiber->cpu);

    return fiber->cpu->cpu_registers[0];
}

EXPORT(SceInt32, _sceFiberInitializeImpl, SceFiber *fiber, const char *name, Ptr<SceFiberEntry> entry, SceUInt32 argOnInitialize, Ptr<void> addrContext, SceSize sizeContext, SceFiberOptParam *params) {
    TRACY_FUNC(_sceFiberInitializeImpl, fiber, name, entry, argOnInitialize, addrContext, sizeContext, params);
    if (!fiber || !entry || !name) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    if ((sizeContext != 0) && (sizeContext < SCE_FIBER_CONTEXT_MINIMUM_SIZE)) {
        return RET_ERROR(SCE_FIBER_ERROR_RANGE);
    }

    if ((!addrContext && (sizeContext != 0)) || (addrContext && (sizeContext == 0)) || ((sizeContext & 7) != 0)) {
        return RET_ERROR(SCE_FIBER_ERROR_INVALID);
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    if (!thread) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }

    initialize_fiber(emuenv, thread, fiber, name, entry, argOnInitialize, addrContext, sizeContext, params);

    return SCE_FIBER_OK;
}

EXPORT(int, _sceFiberInitializeWithInternalOptionImpl, SceFiber *fiber, const char *name, Ptr<SceFiberEntry> entry, SceUInt32 argOnInitialize, Ptr<void> addrContext, SceSize sizeContext) {
    TRACY_FUNC(_sceFiberInitializeWithInternalOptionImpl, fiber, name, entry, argOnInitialize, addrContext, sizeContext);
    if (!fiber || !entry || !name) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    if ((sizeContext != 0) && (sizeContext < SCE_FIBER_CONTEXT_MINIMUM_SIZE)) {
        return RET_ERROR(SCE_FIBER_ERROR_RANGE);
    }

    if ((!addrContext && (sizeContext != 0)) || (addrContext && (sizeContext == 0)) || ((sizeContext & 7) != 0)) {
        return RET_ERROR(SCE_FIBER_ERROR_INVALID);
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    if (!thread) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }

    initialize_fiber(emuenv, thread, fiber, name, entry, argOnInitialize, addrContext, sizeContext, nullptr);

    return SCE_FIBER_OK;
}

EXPORT(SceInt32, sceFiberFinalize, SceFiber *fiber) {
    TRACY_FUNC(sceFiberFinalize, fiber);
    if (!fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    if (fiber->status == FiberStatus::RUN) {
        return RET_ERROR(SCE_FIBER_ERROR_STATE);
    }

    delete fiber->cpu;
    fiber->cpu = nullptr;
    return 0;
}

struct SceFiberInfo {
    Ptr<SceFiberEntry> entry;
    SceUInt32 argOnInitialize;
    Ptr<void> addrContext;
    SceUInt32 sizeContext;
    char name[32];
    SceUInt32 sizeContextMargin;
};

EXPORT(int, sceFiberGetInfo, SceFiber *fiber, SceFiberInfo *fiberInfo) {
    TRACY_FUNC(sceFiberGetInfo, fiber, fiberInfo);
    if (!fiber || !fiberInfo) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }
    fiberInfo->entry = fiber->entry;
    fiberInfo->argOnInitialize = fiber->argOnInitialize;
    fiberInfo->addrContext = fiber->addrContext;
    fiberInfo->sizeContext = fiber->sizeContext;
    memcpy(fiberInfo->name, fiber->name, sizeof(fiberInfo->name));
    STUBBED("sizeContextMargin is stubbed");
    fiberInfo->sizeContextMargin = -1;
    return 0;
}

EXPORT(SceUInt32, sceFiberGetSelf, Ptr<SceFiber> *fiber) {
    TRACY_FUNC(sceFiberGetSelf, fiber);
    const auto state = emuenv.kernel.obj_store.get<FiberState>();
    if (!fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    SceFiber *thread_fiber = get_thread_fiber(*state, thread->id);
    if (thread_fiber)
        *fiber = Ptr<SceFiber>(thread_fiber, emuenv.mem);
    else
        *fiber = Ptr<SceFiber>(0);

    return SCE_FIBER_OK;
}

EXPORT(int, sceFiberOptParamInitialize) {
    TRACY_FUNC(sceFiberOptParamInitialize);
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberPopUserMarkerWithHud) {
    TRACY_FUNC(sceFiberPopUserMarkerWithHud);
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberPushUserMarkerWithHud) {
    TRACY_FUNC(sceFiberPushUserMarkerWithHud);
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberRenameSelf) {
    TRACY_FUNC(sceFiberRenameSelf);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceFiberReturnToThread, uint32_t argOnReturnTo, Ptr<uint32_t> argOnRun) {
    TRACY_FUNC(sceFiberReturnToThread, argOnReturnTo, argOnRun);
    const auto state = emuenv.kernel.obj_store.get<FiberState>();
    const std::lock_guard<std::mutex> lock(state->mutex);
    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    SceFiber *fiber = get_thread_fiber(*state, thread->id);
    if (!fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_PERMISSION);
    }

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
        *(Ptr<uint32_t>(argOnReturn).get(emuenv.mem)) = argOnReturnTo;
    }

    return SCE_FIBER_OK;
}

EXPORT(SceUInt32, sceFiberRun, SceFiber *fiber, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnReturn) {
    TRACY_FUNC(sceFiberRun, fiber, argOnRunTo, argOnReturn);
    const auto state = emuenv.kernel.obj_store.get<FiberState>();
    const std::lock_guard<std::mutex> lock(state->mutex);
    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    if (!fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    if (fiber->status == FiberStatus::RUN) {
        return RET_ERROR(SCE_FIBER_ERROR_STATE);
    }

    if (get_thread_fiber(*state, thread->id)) {
        return RET_ERROR(SCE_FIBER_ERROR_PERMISSION);
    }

    if (LOG_FIBER) {
        log_fiber(*state, thread, fiber, "Run");
    }

    setup_fiber_to_run(emuenv, thread, fiber, read_sp(*thread->cpu), argOnRunTo);
    set_thread_context(*state, thread->id, save_context(*thread->cpu));
    set_thread_fiber(*state, thread->id, fiber);

    load_context(*thread->cpu, *fiber->cpu);
    return fiber->cpu->cpu_registers[0];
}

EXPORT(int, sceFiberStartContextSizeCheck) {
    TRACY_FUNC(sceFiberStartContextSizeCheck);
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberStopContextSizeCheck) {
    TRACY_FUNC(sceFiberStopContextSizeCheck);
    return UNIMPLEMENTED();
}

EXPORT(SceUInt32, sceFiberSwitch, SceFiber *fiber, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
    TRACY_FUNC(sceFiberSwitch, fiber, argOnRunTo, argOnRun);
    const auto state = emuenv.kernel.obj_store.get<FiberState>();
    const std::lock_guard<std::mutex> lock(state->mutex);
    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    auto ctx = get_thread_context(*state, thread->id);
    if (!fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    if (fiber->status == FiberStatus::RUN) {
        return RET_ERROR(SCE_FIBER_ERROR_STATE);
    }

    SceFiber *thread_fiber = get_thread_fiber(*state, thread->id);
    if (!thread_fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_PERMISSION);
    }

    if (LOG_FIBER) {
        log_fiber(*state, thread, fiber, "Switch");
    }

    *thread_fiber->cpu = save_context(*thread->cpu);
    thread_fiber->status = FiberStatus::SUSPEND;
    thread_fiber->argOnRun = argOnRun;
    thread_fiber->cpu->cpu_registers[0] = SCE_FIBER_OK;
    set_thread_fiber(*state, thread->id, fiber);
    setup_fiber_to_run(emuenv, thread, fiber, ctx.get_sp(), argOnRunTo);
    load_context(*thread->cpu, *fiber->cpu);

    return fiber->cpu->cpu_registers[0];
}
