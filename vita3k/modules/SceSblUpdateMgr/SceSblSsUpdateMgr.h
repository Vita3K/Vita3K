// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <kernel/types.h>
#include <module/module.h>

struct SceSblUsSpkgInfo {
    SceSize size;
    int version;
    int reserved1;
    int reserved2;
};
static_assert(sizeof(SceSblUsSpkgInfo) == 0x10);

DECL_EXPORT(int, sceSblUsGetSpkgInfo, int package_type, SceSblUsSpkgInfo *info);