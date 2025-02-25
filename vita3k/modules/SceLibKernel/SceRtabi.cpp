// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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
#include <util/tracy.h>

TRACY_MODULE_NAME(SceRtabi);

EXPORT(int, __rtabi_lcmp) {
    TRACY_FUNC(__rtabi_lcmp);
    return UNIMPLEMENTED();
}

EXPORT(int, __rtabi_lmul) {
    TRACY_FUNC(__rtabi_lmul);
    return UNIMPLEMENTED();
}

EXPORT(int, __rtabi_lasr) {
    TRACY_FUNC(__rtabi_lasr);
    return UNIMPLEMENTED();
}

EXPORT(int, __rtabi_ulcmp) {
    TRACY_FUNC(__rtabi_ulcmp);
    return UNIMPLEMENTED();
}

EXPORT(int32_t, __rtabi_idiv, int32_t numerator, int32_t denominator) {
    TRACY_FUNC(__rtabi_idiv, numerator, denominator);
    if (denominator == 0) {
        if (numerator == 0)
            return 0;
        else if (numerator > 0)
            return INT32_MAX;
        else
            return INT32_MIN;
    }
    return numerator / denominator;
}

EXPORT(int, __rtabi_d2lz) {
    TRACY_FUNC(__rtabi_d2lz);
    return UNIMPLEMENTED();
}

EXPORT(int, __rtabi_f2ulz) {
    TRACY_FUNC(__rtabi_f2ulz);
    return UNIMPLEMENTED();
}

EXPORT(std::div_t, __rtabi_idivmod, int32_t numerator, int32_t denominator) {
    TRACY_FUNC(__rtabi_idivmod);
    if (denominator == 0) {
        if (numerator == 0)
            return std::div_t{ .quot = 0, .rem = 0 };
        else if (numerator > 0)
            return std::div_t{ .quot = INT32_MAX, .rem = 0 };
        else
            return std::div_t{ .quot = INT32_MIN, .rem = 0 };
    }
    return div(numerator, denominator);
}

EXPORT(int, __rtabi_d2ulz) {
    TRACY_FUNC(__rtabi_d2ulz);
    return UNIMPLEMENTED();
}

EXPORT(std::lldiv_t, __rtabi_ldivmod, int64_t numerator, int64_t denominator) {
    TRACY_FUNC(__rtabi_ldivmod, numerator, denominator);
    if (denominator == 0) {
        if (numerator == 0)
            return std::lldiv_t{ .quot = 0, .rem = 0 };
        else if (numerator > 0)
            return std::lldiv_t{ .quot = INT64_MAX, .rem = 0 };
        else
            return std::lldiv_t{ .quot = INT64_MIN, .rem = 0 };
    }
    return lldiv(numerator, denominator);
}

EXPORT(int, __rtabi_llsl) {
    TRACY_FUNC(__rtabi_llsl);
    return UNIMPLEMENTED();
}

EXPORT(int, __rtabi_f2lz) {
    TRACY_FUNC(__rtabi_f2lz);
    return UNIMPLEMENTED();
}

EXPORT(int, __rtabi_uidivmod, uint32_t numerator, uint32_t denominator) {
    TRACY_FUNC(__rtabi_uidivmod, numerator, denominator);
    return UNIMPLEMENTED();
}

EXPORT(int, __rtabi_llsr) {
    TRACY_FUNC(__rtabi_llsr);
    return UNIMPLEMENTED();
}

EXPORT(std::lldiv_t, __rtabi_uldivmod, uint64_t numerator, uint64_t denominator) {
    TRACY_FUNC(__rtabi_uldivmod, numerator, denominator);
    if (denominator == 0) {
        if (numerator == 0)
            return std::lldiv_t{ .quot = 0, .rem = 0 };
        else
            return std::lldiv_t{ .quot = -1, .rem = 0 };
    }
    return lldiv(numerator, denominator);
}

EXPORT(uint32_t, __rtabi_uidiv, uint32_t numerator, uint32_t denominator) {
    TRACY_FUNC(__rtabi_uidiv, numerator, denominator);
    if (denominator == 0) {
        if (numerator == 0)
            return 0;
        else
            return UINT32_MAX;
    }
    return numerator / denominator;
}
