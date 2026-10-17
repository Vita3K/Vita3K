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

#include "SceSblAimgr.h"
#include <io/io.h>
#include <kernel/types.h>
#include <pfsmgr/functions.h>
#include <util/log.h>

#include <algorithm>
#include <new>
#include <stdexcept>

EXPORT(int, _sceKernelGetOpenPsId, uint8_t *id) {
    if (!id)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    try {
        const auto open_psid = pfsmgr::get_or_create_open_psid(emuenv.vita_fs_path / "ux0");
        std::copy(open_psid.begin(), open_psid.end(), id);
        return 0;
    } catch (const std::runtime_error &error) {
        LOG_ERROR("Cannot load emulated OpenPSID: {}", error.what());
        return RET_ERROR(SCE_ERROR_ERRNO_EIO);
    } catch (const std::bad_alloc &) {
        LOG_ERROR("Cannot load emulated OpenPSID: out of memory");
        return RET_ERROR(SCE_ERROR_ERRNO_ENOMEM);
    }
}
