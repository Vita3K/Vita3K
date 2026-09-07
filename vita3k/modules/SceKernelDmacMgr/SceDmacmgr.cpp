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

#include <cpu/functions.h>
#include <kernel/state.h>
#include <mem/functions.h>
#include <module/module.h>
#include <util/tracy.h>
TRACY_MODULE_NAME(SceDmacmgr);

EXPORT(int, sceDmacMemcpy, Ptr<void> dst, const void *src, SceSize len) {
    TRACY_FUNC(sceDmacMemcpy, dst, src, len);
    if (len && watch_intersects(dst.address(), len)) {
        const ThreadStatePtr wt = emuenv.kernel.get_thread(thread_id);
        LOG_CRITICAL("[watch] sceDmacMemcpy(0x{:08x}, {}, {}) thread={} lr=0x{:08x}", dst.address(), fmt::ptr(src), len, thread_id, wt ? read_lr(*wt->cpu) : 0);
    }
    memcpy(dst.get(emuenv.mem), src, len);
    return 0;
}

EXPORT(int, sceDmacMemset, Ptr<void> dst, int ch, SceSize len) {
    TRACY_FUNC(sceDmacMemset, dst, ch, len);
    if (len && watch_intersects(dst.address(), len)) {
        const ThreadStatePtr wt = emuenv.kernel.get_thread(thread_id);
        LOG_CRITICAL("[watch] sceDmacMemset(0x{:08x}, 0x{:x}, {}) thread={} lr=0x{:08x}", dst.address(), ch, len, thread_id, wt ? read_lr(*wt->cpu) : 0);
    }
    memset(dst.get(emuenv.mem), ch, len);
    return 0;
}
