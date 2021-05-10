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

struct InitialFiber {
    Address start;
    Address end;
    SceFiber *fiber;
};

typedef std::vector<InitialFiber> InitialFibers;

struct FiberState {
    InitialFibers initial_fibers;
};

LIBRARY_INIT_IMPL(SceFiber) {
    host.kernel.obj_store.create<FiberState>();
}
LIBRARY_INIT_REGISTER(SceFiber)

std::string _describeFiber(ThreadStatePtr thread, SceFiber *fiber) {
    std::stringstream ss;
    ss << fmt::format("Fiber (name: {})\n", fiber->name);
    ss << fmt::format("entry: {}\n", log_hex(fiber->cpu->get_pc()), log_hex(fiber->entry.address()));
    ss << "CPU Context:\n";
    ss << fiber->cpu->description();
    ss << "Referenced from " << thread->id << "\n";
    ss << "CPU Context:\n";
    ss << thread->cpu_context.description();
    return ss.str();
}

InitialFiber *_findIntialFiber(FiberState &state, Address sp) {
    // TODO use interval tree
    // TODO destroy initial fibers
    for (auto &ifiber : state.initial_fibers) {
        // stack adress is descending
        if (ifiber.start <= sp && sp <= ifiber.end) {
            return &ifiber;
        }
    }
    return nullptr;
}

void _resetFiber(FiberState &state, MemState &mem, SceFiber *fiber) {
    auto ifiber = _findIntialFiber(state, fiber->cpu->get_sp());
    assert(ifiber);
    const auto cpu = fiber->cpu;
    *fiber = *ifiber->fiber;
    fiber->cpu = cpu;
    *fiber->cpu = *ifiber->fiber->cpu;
    memset(Ptr<void>(ifiber->start).get(mem), 0xCC, ifiber->end - ifiber->start);
}

int _fiberSwitch(HostState &host, const ThreadStatePtr thread, SceFiber *fiber, CPUContext &backup_cpu_context, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun, bool reset) {
    const auto state = host.kernel.obj_store.get<FiberState>();
    backup_cpu_context = save_context(*thread->cpu);

    bool suspended = false;
    if (fiber->addrContext == 0 && reset) {
        _resetFiber(*state, host.mem, fiber);
    } else if (fiber->entry.address() == fiber->cpu->get_pc()) {
        fiber->cpu->cpu_registers[1] = argOnRunTo;
    } else {
        suspended = true;
        Address previousArgOnRun = fiber->cpu->cpu_registers[2];
        if (previousArgOnRun) {
            *(Ptr<SceUInt32>(previousArgOnRun).get(host.mem)) = argOnRunTo;
        }
    }

    load_context(*(thread->cpu), *fiber->cpu);

    thread->fiber = Ptr<void>(fiber, host.mem);

    return suspended ? SCE_FIBER_OK : fiber->cpu->cpu_registers[0];
}

void _initializeFiber(HostState &host, const ThreadStatePtr thread, SceFiber *fiber, const char *name, Ptr<SceFiberEntry> entry, SceUInt32 argOnInitialize, Ptr<void> addrContext, SceSize sizeContext, SceFiberOptParam *params) {
    const auto state = host.kernel.obj_store.get<FiberState>();
    fiber->entry = entry;
    strncpy(fiber->name, name, 32);
    fiber->argOnInitialize = argOnInitialize;
    fiber->addrContext = addrContext.address();
    fiber->sizeContext = sizeContext;
    fiber->cpu = new CPUContext;
    *fiber->cpu = save_context(*thread->cpu);

    fiber->cpu->cpu_registers[0] = argOnInitialize;
    fiber->cpu->set_pc(fiber->entry.address());

    if (addrContext.address() == 0) {
        auto ctx_name = new char[32];
        strncpy(ctx_name, name, 32);

        addrContext = Ptr<void>(alloc(host.mem, DEFAULT_FIBER_STACK_SIZE, ctx_name));
        sizeContext = DEFAULT_FIBER_STACK_SIZE;
    }

    fiber->cpu->set_sp(addrContext.address() + sizeContext);
    memset(addrContext.get(host.mem), 0xCC, sizeContext);

    SceFiber *fiberCopy = new SceFiber;
    *fiberCopy = *fiber;
    fiberCopy->cpu = new CPUContext;
    *fiberCopy->cpu = *fiber->cpu;

    InitialFiber ifiber;
    ifiber.start = addrContext.address();
    ifiber.end = addrContext.address() + sizeContext;
    ifiber.fiber = fiberCopy;
    state->initial_fibers.push_back(ifiber);
}

EXPORT(int, _sceFiberAttachContextAndRun, SceFiber *fiber, Address addrContext, SceSize sizeContext, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
    // Maybe Need more check on real hw
    STUBBED("Todo: not sure for now");
    const auto thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    fiber->addrContext = addrContext;
    fiber->sizeContext = sizeContext;
    auto res = _fiberSwitch(host, thread, fiber, thread->cpu_context, argOnRunTo, argOnRun, false);
    write_lr(*(thread->cpu), thread->cpu_context.get_lr());

    return res;
}

EXPORT(int, _sceFiberAttachContextAndSwitch, SceFiber *fiber, Address addrContext, SceSize sizeContext, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
    // Maybe Need more check on real hw
    STUBBED("Todo: not sure for now");
    const auto thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    auto old_fiber = thread->fiber.cast<SceFiber>().get(host.mem);
    fiber->addrContext = addrContext;
    fiber->sizeContext = sizeContext;

    return _fiberSwitch(host, thread, fiber, *old_fiber->cpu, argOnRunTo, argOnRun, true);
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

    _initializeFiber(host, thread, fiber, name, entry, argOnInitialize, addrContext, sizeContext, params);

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

    _initializeFiber(host, thread, fiber, name, entry, argOnInitialize, addrContext, sizeContext, nullptr);

    return SCE_FIBER_OK;
}

EXPORT(SceInt32, sceFiberFinalize, SceFiber *fiber) {
    if (!fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    delete fiber->cpu;
    return STUBBED("TODO CHECKS");
}

EXPORT(int, sceFiberGetInfo) {
    return UNIMPLEMENTED();
}

EXPORT(SceUInt32, sceFiberGetSelf, Ptr<SceFiber> *fiber) {
    if (!fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    if (thread->fiber)
        *fiber = thread->fiber.cast<SceFiber>();
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

EXPORT(SceInt32, sceFiberReturnToThread, SceUInt32 argOnReturn, Ptr<SceUInt32> argOnRun) {
    const auto state = host.kernel.obj_store.get<FiberState>();
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    auto fiber = thread->fiber.cast<SceFiber>().get(host.mem);
    (*thread->fiber.cast<SceFiber>().get(host.mem)->cpu) = save_context(*thread->cpu);
    load_context(*thread->cpu, thread->cpu_context);

    Address previousArgOnRun = thread->cpu_context.cpu_registers[2];
    if (fiber->addrContext == 0) {
        _resetFiber(*state, host.mem, fiber);
    } else if (previousArgOnRun) {
        *(Ptr<SceUInt32>(previousArgOnRun).get(host.mem)) = argOnReturn;
    }

    return SCE_FIBER_OK;
}

EXPORT(SceUInt32, sceFiberRun, SceFiber *fiber, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
    if (!fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    auto res = _fiberSwitch(host, thread, fiber, thread->cpu_context, argOnRunTo, argOnRun, false);
    write_lr(*(thread->cpu), thread->cpu_context.get_lr());
    return res;
}

EXPORT(int, sceFiberStartContextSizeCheck) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberStopContextSizeCheck) {
    return UNIMPLEMENTED();
}

EXPORT(SceUInt32, sceFiberSwitch, SceFiber *fiber, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
    if (!fiber) {
        return RET_ERROR(SCE_FIBER_ERROR_NULL);
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    auto old_fiber = thread->fiber.cast<SceFiber>().get(host.mem);
    return _fiberSwitch(host, thread, fiber, *old_fiber->cpu, argOnRunTo, argOnRun, true);
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
