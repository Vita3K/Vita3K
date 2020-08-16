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

#include <util/lock_and_find.h>
#include <util/log.h>

const static int DEFAULT_FIBER_STACK_SIZE = 4096;

EXPORT(int, _sceFiberAttachContextAndRun) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiberAttachContextAndSwitch) {
    return UNIMPLEMENTED();
}

InitialFiber *_findIntialFiber(KernelState &kernel, Address sp) {
    // TODO use interval tree
    // TODO destroy initial fibers
    for (auto &ifiber : kernel.initial_fibers) {
        // stack adress is descending
        if (ifiber.start <= sp && sp <= ifiber.end) {
            return &ifiber;
        }
    }
    return nullptr;
}

void _resetFiber(HostState &host, SceFiber *fiber) {
    auto ifiber = _findIntialFiber(host.kernel, fiber->cpu->cpu_registers[13]);
    assert(ifiber);
    delete fiber->cpu;
    *fiber = *ifiber->fiber;
    fiber->cpu = new CPUContext;
    *fiber->cpu = *ifiber->fiber->cpu;
    memset(Ptr<void>(ifiber->start).get(host.mem), 0xCC, ifiber->end - ifiber->start);
}

int _fiberSwitch(HostState &host, const ThreadStatePtr thread, SceFiber *fiber, CPUContext& backup_cpu_context, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun, bool reset) {
    backup_cpu_context = save_context(*thread->cpu);

    bool suspended = false;
    if (fiber->addrContext == 0 && reset) {
        _resetFiber(host, fiber);
    } else if (fiber->entry.address() == fiber->cpu->cpu_registers[15]) {
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

void _initializeFiber(HostState &host, const ThreadStatePtr thread, SceFiber *fiber, char *name, Ptr<SceFiberEntry> entry, SceUInt32 argOnInitialize, Ptr<void> addrContext, SceSize sizeContext, SceFiberOptParam *params) {
    fiber->entry = entry;
    strncpy(fiber->name, name, 32);
    fiber->argOnInitialize = argOnInitialize;
    fiber->addrContext = addrContext.address();
    fiber->sizeContext = sizeContext;
    auto ctx = new CPUContext;
    fiber->cpu = ctx;
    *ctx = save_context(*thread->cpu);
    ctx->cpu_registers[0] = argOnInitialize;
    ctx->cpu_registers[15] = fiber->entry.address();
    /*if (fiber->entry.address() & 1) {
        ctx->cpsr = 0x20;
    } else {
        ctx->cpsr = 0;
    }*/

    if (addrContext.address() == 0) {
        auto ctx_name = new char[32];
        strncpy(ctx_name, name, 32);

        addrContext = Ptr<void>(alloc(host.mem, DEFAULT_FIBER_STACK_SIZE, ctx_name));
        sizeContext = DEFAULT_FIBER_STACK_SIZE;
    }

    ctx->cpu_registers[13] = addrContext.address() + sizeContext;
    memset(addrContext.get(host.mem), 0xCC, sizeContext);

    SceFiber *fiberCopy = new SceFiber;
    *fiberCopy = *fiber;
    memcpy(fiberCopy, fiber, sizeof(SceFiber));
    fiberCopy->cpu = new CPUContext;
    *fiberCopy->cpu = *ctx;

    InitialFiber ifiber;
    ifiber.start = addrContext.address();
    ifiber.end = addrContext.address() + sizeContext;
    ifiber.fiber = fiberCopy;
    host.kernel.initial_fibers.push_back(ifiber);
}

EXPORT(SceInt32, _sceFiberInitializeImpl, SceFiber *fiber, char *name, Ptr<SceFiberEntry> entry, SceUInt32 argOnInitialize, Ptr<void> addrContext, SceSize sizeContext, SceFiberOptParam *params) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    _initializeFiber(host, thread, fiber, name, entry, argOnInitialize, addrContext, sizeContext, params);

    return SCE_FIBER_OK;
}

EXPORT(int, _sceFiberInitializeWithInternalOptionImpl, SceFiber *fiber, char *name, Ptr<SceFiberEntry> entry, SceUInt32 argOnInitialize, Ptr<void> addrContext, SceSize sizeContext) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    _initializeFiber(host, thread, fiber, name, entry, argOnInitialize, addrContext, sizeContext, nullptr);

    return SCE_FIBER_OK;
}

EXPORT(SceInt32, sceFiberFinalize, SceFiber *fiber) {
    return STUBBED("TODO CHECKS");
}

EXPORT(int, sceFiberGetInfo) {
    return UNIMPLEMENTED();
}

EXPORT(SceUInt32, sceFiberGetSelf, Ptr<SceFiber> *fiber) {
    if (!fiber)
        return SCE_FIBER_ERROR_NULL;

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
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    auto fiber = thread->fiber.cast<SceFiber>().get(host.mem);
    *fiber->cpu = save_context(*thread->cpu);
    load_context(*thread->cpu, thread->cpu_context);

    Address previousArgOnRun = thread->cpu_context.cpu_registers[2];
    if (fiber->addrContext == 0) {
        _resetFiber(host, fiber);
    } else if (previousArgOnRun) {
        *(Ptr<SceUInt32>(previousArgOnRun).get(host.mem)) = argOnReturn;
    }

    return SCE_FIBER_OK;
}

EXPORT(SceUInt32, sceFiberRun, SceFiber *fiber, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    auto res = _fiberSwitch(host, thread, fiber, thread->cpu_context, argOnRunTo, argOnRun, false);
    write_lr(*(thread->cpu), thread->cpu_context.cpu_registers[14]); // lr
    return res;
}

EXPORT(int, sceFiberStartContextSizeCheck) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberStopContextSizeCheck) {
    return UNIMPLEMENTED();
}

EXPORT(SceUInt32, sceFiberSwitch, SceFiber *fiber, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
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
