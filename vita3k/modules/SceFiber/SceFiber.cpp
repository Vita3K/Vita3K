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

typedef void(SceFiberEntry)(SceUInt32 argOnInitialize, SceUInt32 argOnRun);

struct SceFiberOptParam {
    char reserved[128];
};

typedef struct SceFiber {
    Ptr<SceFiberEntry> entry;
    SceUInt32 argOnInitialize;
    Address addrContext;
    SceSize sizeContext;
    char name[32];
    CPUContext cpu;
} SceFiber;

EXPORT(int, _sceFiberAttachContextAndRun) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceFiberAttachContextAndSwitch) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceFiberInitializeImpl, SceFiber *fiber, char *name, Ptr<SceFiberEntry> entry, SceUInt32 argOnInitialize, Ptr<void> addrContext, SceSize sizeContext, SceFiberOptParam *params) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    fiber->entry = entry;
    strncpy(fiber->name, name, 32);
    fiber->argOnInitialize = argOnInitialize;
    fiber->addrContext = addrContext.address();
    fiber->sizeContext = sizeContext;
    save_context(*(thread->cpu), fiber->cpu);
    fiber->cpu.cpu_registers[0] = argOnInitialize;
    fiber->cpu.pc = fiber->entry.address();
    fiber->cpu.sp = fiber->addrContext + sizeContext;
    memset(addrContext.get(host.mem), 0xCC, sizeContext);
    return SCE_KERNEL_OK;
}

EXPORT(int, _sceFiberInitializeWithInternalOptionImpl) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceFiberFinalize, SceFiber *fiber) {
    return STUBBED("TODO CHECKS");
}

EXPORT(int, sceFiberGetInfo) {
    return UNIMPLEMENTED();
}

EXPORT(SceUInt32, sceFiberGetSelf, SceFiber *fiber) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    *fiber = *thread->fiber.cast<SceFiber>().get(host.mem);

    return 0;
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

    save_context(*(thread->cpu), (thread->fiber.cast<SceFiber>().get(host.mem)->cpu));

    load_context(*(thread->cpu), *(thread->cpu_context));

    Address previousArgOnRun = thread->cpu_context->cpu_registers[2];
    if (previousArgOnRun) {
        *(Ptr<SceUInt32>(previousArgOnRun).get(host.mem)) = argOnReturn;
    }

    return SCE_KERNEL_OK;
}

EXPORT(SceUInt32, sceFiberRun, Ptr<SceFiber> fiber_ptr, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    save_context(*(thread->cpu), *(thread->cpu_context));

    bool suspended = false;

    SceFiber *fiber = fiber_ptr.get(host.mem);

    if (fiber->entry.address() == fiber->cpu.pc) {
        fiber->cpu.cpu_registers[1] = argOnRunTo;
    } else {
        suspended = true;
        Address previousArgOnRun = fiber->cpu.cpu_registers[2];
        if (previousArgOnRun) {
            *(Ptr<SceUInt32>(previousArgOnRun).get(host.mem)) = argOnRunTo;
        }
    }

    load_context(*(thread->cpu), fiber->cpu);
    write_lr(*(thread->cpu), thread->cpu_context.get()->lr);
    thread->fiber = fiber_ptr.cast<void>();

    return suspended ? SCE_KERNEL_OK : fiber->cpu.cpu_registers[0];
}

EXPORT(int, sceFiberStartContextSizeCheck) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceFiberStopContextSizeCheck) {
    return UNIMPLEMENTED();
}

EXPORT(SceUInt32, sceFiberSwitch, Ptr<SceFiber> fiber_ptr, SceUInt32 argOnRunTo, Ptr<SceUInt32> argOnRun) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    save_context(*(thread->cpu), (thread->fiber.cast<SceFiber>().get(host.mem)->cpu));

    bool suspended = false;

    SceFiber *fiber = fiber_ptr.get(host.mem);

    if (fiber->entry.address() == fiber->cpu.pc) {
        fiber->cpu.cpu_registers[1] = argOnRunTo;
    } else {
        suspended = true;
        Address previousArgOnRun = fiber->cpu.cpu_registers[2];
        if (previousArgOnRun) {
            *(Ptr<SceUInt32>(previousArgOnRun).get(host.mem)) = argOnRunTo;
        }
    }

    load_context(*(thread->cpu), fiber->cpu);

    thread->fiber = fiber_ptr.cast<void>();

    return suspended ? SCE_KERNEL_OK : fiber->cpu.cpu_registers[0];
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
