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

#include <module/module.h>

#include <kernel/state.h>
#include <util/log.h>
#include <util/tracy.h>

#include <v3kprintf.h>

TRACY_MODULE_NAME(SceSysclibForDriver);

EXPORT(int, __aeabi_idiv, SceInt numerator, SceInt denominator) {
    TRACY_FUNC(__aeabi_idiv, numerator, denominator);
    return numerator / denominator;
}

EXPORT(int, __aeabi_lcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_ldivmod) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_lmul) {
    return UNIMPLEMENTED();
}

EXPORT(SceUInt, __aeabi_uidiv, SceUInt a, SceUInt b) {
    TRACY_FUNC(__aeabi_uidiv, a, b);
    return a / b;
}

EXPORT(std::div_t, __aeabi_uidivmod, SceInt numerator, SceInt denominator) {
    TRACY_FUNC(__aeabi_uidivmod, numerator, denominator);
    if (denominator == 0) {
        if (numerator == 0)
            return std::div_t{ .quot = 0, .rem = 0 };
        else
            return std::div_t{ .quot = INT32_MIN, .rem = 0 };
    }
    if (numerator > 0 && denominator > 0) {
        return std::div(numerator, denominator);
    }
    uint32_t unom = std::bit_cast<uint32_t>(numerator);
    uint32_t udev = std::bit_cast<uint32_t>(denominator);
    uint32_t uquot = unom / udev;
    uint32_t urem = unom % udev;
    std::div_t res{ .quot = std::bit_cast<int32_t>(uquot), .rem = std::bit_cast<int32_t>(urem) };
    return res;
}

EXPORT(std::lldiv_t, __aeabi_uldivmod, SceInt64 numerator, SceInt64 denominator) {
    TRACY_FUNC(__aeabi_uldivmod, numerator, denominator);
    if (denominator == 0) {
        if (numerator == 0)
            return std::lldiv_t{ .quot = 0, .rem = 0 };
        else
            return std::lldiv_t{ .quot = INT64_MIN, .rem = 0 };
    }
    if (numerator > 0 && denominator > 0) {
        return std::lldiv(numerator, denominator);
    }
    uint64_t unom = std::bit_cast<uint64_t>(numerator);
    uint64_t udev = std::bit_cast<uint64_t>(denominator);
    uint64_t uquot = unom / udev;
    uint64_t urem = unom % udev;
    std::lldiv_t res{ .quot = std::bit_cast<int64_t>(uquot), .rem = std::bit_cast<int64_t>(urem) };
    return res;
}

EXPORT(int, __aeabi_ulcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, __memcpy_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, __memmove_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, __memset_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, __kstack_chk_fail) {
    return UNIMPLEMENTED();
}

EXPORT(int, __strncat_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, __strncpy_chk) {
    return UNIMPLEMENTED();
}

EXPORT(int, look_ctype_table) {
    return UNIMPLEMENTED();
}

EXPORT(int, kmemchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, kmemcmp) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, kmemcpy, Ptr<void> dst, Ptr<const void> src, SceSize size) {
    TRACY_FUNC(kmemcpy, dst, src, size);
    if (dst.address() == src.address() || size == 0) {
        return dst; // No operation needed
    }
    auto res = memcpy(dst.get(emuenv.mem), src.get(emuenv.mem), size);
    if (res == nullptr) {
        return {}; // Error occurred
    }
    return dst; // Success
}

EXPORT(int, kmemmove) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, kmemset, Ptr<void> dst, int val, SceSize size) {
    TRACY_FUNC(kmemset, dst, val, size);
    memset(dst.get(emuenv.mem), val, size);
    return dst;
}

EXPORT(int, rshift) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksnprintf, char *s, size_t n, const char *format, module::vargs args) {
    // TODO: add args to tracy func
    TRACY_FUNC(ksnprintf, s, n, format);

    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    return utils::snprintf(s, n, format, *(thread->cpu), emuenv.mem, args);
}

EXPORT(int, kstrchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, strlcat) {
    return UNIMPLEMENTED();
}

EXPORT(int, strlcpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrlen, const char *s) {
    TRACY_FUNC(kstrlen, s);
    if (!s) {
        return 0; // Handle null pointer
    }
    return strlen(s);
}

EXPORT(int, kstrncat) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrncmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrncpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, strnlen) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrrchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrstr) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrtol) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrtoll) {
    return UNIMPLEMENTED();
}

EXPORT(int, kstrtoul) {
    return UNIMPLEMENTED();
}

EXPORT(int, ktolower) {
    return UNIMPLEMENTED();
}

EXPORT(int, ktoupper) {
    return UNIMPLEMENTED();
}

EXPORT(int, kvsnprintf) {
    return UNIMPLEMENTED();
}
