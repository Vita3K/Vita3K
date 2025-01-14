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
TRACY_MODULE_NAME(SceLibm);

EXPORT(int, _Cosh) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Dclass) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Dsign) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Dtest) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Exp) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FCosh) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FDclass) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FDsign) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FDtest) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FExp) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FFpcomp) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FLog) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FSin) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FSinh) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FSinx, float a, unsigned int tag, int c) {
    TRACY_FUNC(_FSinx, a, tag, c);
    if (tag == 1)
        return static_cast<int>(cosf(a));
    else
        return static_cast<int>(sinf(a));
}

EXPORT(int, _Fpcomp) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LCosh) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LDclass) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LDsign) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LDtest) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LExp) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LFpcomp) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LLog) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LSin) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LSinh) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LSinx) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Log) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Sin) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Sinh) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Sinx) {
    return UNIMPLEMENTED();
}

EXPORT(int, acos) {
    return UNIMPLEMENTED();
}

EXPORT(int, acosf) {
    return UNIMPLEMENTED();
}

EXPORT(int, acosh) {
    return UNIMPLEMENTED();
}

EXPORT(int, acoshf) {
    return UNIMPLEMENTED();
}

EXPORT(int, acoshl) {
    return UNIMPLEMENTED();
}

EXPORT(int, acosl) {
    return UNIMPLEMENTED();
}

EXPORT(int, asin) {
    return UNIMPLEMENTED();
}

EXPORT(int, asinf) {
    return UNIMPLEMENTED();
}

EXPORT(int, asinh) {
    return UNIMPLEMENTED();
}

EXPORT(int, asinhf) {
    return UNIMPLEMENTED();
}

EXPORT(int, asinhl) {
    return UNIMPLEMENTED();
}

EXPORT(int, asinl) {
    return UNIMPLEMENTED();
}

EXPORT(int, atan) {
    return UNIMPLEMENTED();
}

EXPORT(int, atan2) {
    return UNIMPLEMENTED();
}

EXPORT(int, atan2f) {
    return UNIMPLEMENTED();
}

EXPORT(int, atan2l) {
    return UNIMPLEMENTED();
}

EXPORT(int, atanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, atanh) {
    return UNIMPLEMENTED();
}

EXPORT(int, atanhf) {
    return UNIMPLEMENTED();
}

EXPORT(int, atanhl) {
    return UNIMPLEMENTED();
}

EXPORT(int, atanl) {
    return UNIMPLEMENTED();
}

EXPORT(int, cbrt) {
    return UNIMPLEMENTED();
}

EXPORT(int, cbrtf) {
    return UNIMPLEMENTED();
}

EXPORT(int, cbrtl) {
    return UNIMPLEMENTED();
}

EXPORT(int, ceil) {
    return UNIMPLEMENTED();
}

EXPORT(int, ceilf) {
    return UNIMPLEMENTED();
}

EXPORT(int, ceill) {
    return UNIMPLEMENTED();
}

EXPORT(int, copysign) {
    return UNIMPLEMENTED();
}

EXPORT(int, copysignf) {
    return UNIMPLEMENTED();
}

EXPORT(int, copysignl) {
    return UNIMPLEMENTED();
}

EXPORT(int, cos) {
    return UNIMPLEMENTED();
}

EXPORT(int, cosf) {
    return UNIMPLEMENTED();
}

EXPORT(int, cosh) {
    return UNIMPLEMENTED();
}

EXPORT(int, coshf) {
    return UNIMPLEMENTED();
}

EXPORT(int, coshl) {
    return UNIMPLEMENTED();
}

EXPORT(int, cosl) {
    return UNIMPLEMENTED();
}

EXPORT(int, erf) {
    return UNIMPLEMENTED();
}

EXPORT(int, erfc) {
    return UNIMPLEMENTED();
}

EXPORT(int, erfcf) {
    return UNIMPLEMENTED();
}

EXPORT(int, erfcl) {
    return UNIMPLEMENTED();
}

EXPORT(int, erff) {
    return UNIMPLEMENTED();
}

EXPORT(int, erfl) {
    return UNIMPLEMENTED();
}

EXPORT(int, exp) {
    return UNIMPLEMENTED();
}

EXPORT(int, exp2) {
    return UNIMPLEMENTED();
}

EXPORT(int, exp2f) {
    return UNIMPLEMENTED();
}

EXPORT(int, exp2l) {
    return UNIMPLEMENTED();
}

EXPORT(int, expf) {
    return UNIMPLEMENTED();
}

EXPORT(int, expl) {
    return UNIMPLEMENTED();
}

EXPORT(int, expm1) {
    return UNIMPLEMENTED();
}

EXPORT(int, expm1f) {
    return UNIMPLEMENTED();
}

EXPORT(int, expm1l) {
    return UNIMPLEMENTED();
}

EXPORT(int, fabs) {
    return UNIMPLEMENTED();
}

EXPORT(int, fabsf) {
    return UNIMPLEMENTED();
}

EXPORT(int, fabsl) {
    return UNIMPLEMENTED();
}

EXPORT(int, fdim) {
    return UNIMPLEMENTED();
}

EXPORT(int, fdimf) {
    return UNIMPLEMENTED();
}

EXPORT(int, fdiml) {
    return UNIMPLEMENTED();
}

EXPORT(int, floor) {
    return UNIMPLEMENTED();
}

EXPORT(int, floorf) {
    return UNIMPLEMENTED();
}

EXPORT(int, floorl) {
    return UNIMPLEMENTED();
}

EXPORT(int, fma) {
    return UNIMPLEMENTED();
}

EXPORT(int, fmaf) {
    return UNIMPLEMENTED();
}

EXPORT(int, fmal) {
    return UNIMPLEMENTED();
}

EXPORT(int, fmax) {
    return UNIMPLEMENTED();
}

EXPORT(int, fmaxf) {
    return UNIMPLEMENTED();
}

EXPORT(int, fmaxl) {
    return UNIMPLEMENTED();
}

EXPORT(int, fmin) {
    return UNIMPLEMENTED();
}

EXPORT(int, fminf) {
    return UNIMPLEMENTED();
}

EXPORT(int, fminl) {
    return UNIMPLEMENTED();
}

EXPORT(int, fmod) {
    return UNIMPLEMENTED();
}

EXPORT(int, fmodf) {
    return UNIMPLEMENTED();
}

EXPORT(int, fmodl) {
    return UNIMPLEMENTED();
}

EXPORT(int, frexp) {
    return UNIMPLEMENTED();
}

EXPORT(int, frexpf) {
    return UNIMPLEMENTED();
}

EXPORT(int, frexpl) {
    return UNIMPLEMENTED();
}

EXPORT(int, hypot) {
    return UNIMPLEMENTED();
}

EXPORT(int, hypotf) {
    return UNIMPLEMENTED();
}

EXPORT(int, hypotl) {
    return UNIMPLEMENTED();
}

EXPORT(int, ilogb) {
    return UNIMPLEMENTED();
}

EXPORT(int, ilogbf) {
    return UNIMPLEMENTED();
}

EXPORT(int, ilogbl) {
    return UNIMPLEMENTED();
}

EXPORT(int, ldexp) {
    return UNIMPLEMENTED();
}

EXPORT(int, ldexpf) {
    return UNIMPLEMENTED();
}

EXPORT(int, ldexpl) {
    return UNIMPLEMENTED();
}

EXPORT(int, lgamma) {
    return UNIMPLEMENTED();
}

EXPORT(int, lgammaf) {
    return UNIMPLEMENTED();
}

EXPORT(int, lgammal) {
    return UNIMPLEMENTED();
}

EXPORT(int, llrint) {
    return UNIMPLEMENTED();
}

EXPORT(int, llrintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, llrintl) {
    return UNIMPLEMENTED();
}

EXPORT(int, llround) {
    return UNIMPLEMENTED();
}

EXPORT(int, llroundf) {
    return UNIMPLEMENTED();
}

EXPORT(int, llroundl) {
    return UNIMPLEMENTED();
}

EXPORT(int, log) {
    return UNIMPLEMENTED();
}

EXPORT(int, log10) {
    return UNIMPLEMENTED();
}

EXPORT(int, log10f) {
    return UNIMPLEMENTED();
}

EXPORT(int, log10l) {
    return UNIMPLEMENTED();
}

EXPORT(int, log1p) {
    return UNIMPLEMENTED();
}

EXPORT(int, log1pf) {
    return UNIMPLEMENTED();
}

EXPORT(int, log1pl) {
    return UNIMPLEMENTED();
}

EXPORT(int, log2) {
    return UNIMPLEMENTED();
}

EXPORT(int, log2f) {
    return UNIMPLEMENTED();
}

EXPORT(int, log2l) {
    return UNIMPLEMENTED();
}

EXPORT(int, logb) {
    return UNIMPLEMENTED();
}

EXPORT(int, logbf) {
    return UNIMPLEMENTED();
}

EXPORT(int, logbl) {
    return UNIMPLEMENTED();
}

EXPORT(int, logf) {
    return UNIMPLEMENTED();
}

EXPORT(int, logl) {
    return UNIMPLEMENTED();
}

EXPORT(int, lrint) {
    return UNIMPLEMENTED();
}

EXPORT(int, lrintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, lrintl) {
    return UNIMPLEMENTED();
}

EXPORT(int, lround) {
    return UNIMPLEMENTED();
}

EXPORT(int, lroundf) {
    return UNIMPLEMENTED();
}

EXPORT(int, lroundl) {
    return UNIMPLEMENTED();
}

EXPORT(int, modf) {
    return UNIMPLEMENTED();
}

EXPORT(int, modff) {
    return UNIMPLEMENTED();
}

EXPORT(int, modfl) {
    return UNIMPLEMENTED();
}

EXPORT(int, nan) {
    return UNIMPLEMENTED();
}

EXPORT(int, nanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, nanl) {
    return UNIMPLEMENTED();
}

EXPORT(int, nearbyint) {
    return UNIMPLEMENTED();
}

EXPORT(int, nearbyintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, nearbyintl) {
    return UNIMPLEMENTED();
}

EXPORT(int, nextafter) {
    return UNIMPLEMENTED();
}

EXPORT(int, nextafterf) {
    return UNIMPLEMENTED();
}

EXPORT(int, nextafterl) {
    return UNIMPLEMENTED();
}

EXPORT(int, nexttoward) {
    return UNIMPLEMENTED();
}

EXPORT(int, nexttowardf) {
    return UNIMPLEMENTED();
}

EXPORT(int, nexttowardl) {
    return UNIMPLEMENTED();
}

EXPORT(int, pow) {
    return UNIMPLEMENTED();
}

EXPORT(int, powf) {
    return UNIMPLEMENTED();
}

EXPORT(int, powl) {
    return UNIMPLEMENTED();
}

EXPORT(int, remainder) {
    return UNIMPLEMENTED();
}

EXPORT(int, remainderf) {
    return UNIMPLEMENTED();
}

EXPORT(int, remainderl) {
    return UNIMPLEMENTED();
}

EXPORT(int, remquo) {
    return UNIMPLEMENTED();
}

EXPORT(int, remquof) {
    return UNIMPLEMENTED();
}

EXPORT(int, remquol) {
    return UNIMPLEMENTED();
}

EXPORT(int, rint) {
    return UNIMPLEMENTED();
}

EXPORT(int, rintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, rintl) {
    return UNIMPLEMENTED();
}

EXPORT(int, round) {
    return UNIMPLEMENTED();
}

EXPORT(int, roundf) {
    return UNIMPLEMENTED();
}

EXPORT(int, roundl) {
    return UNIMPLEMENTED();
}

EXPORT(int, scalbln) {
    return UNIMPLEMENTED();
}

EXPORT(int, scalblnf) {
    return UNIMPLEMENTED();
}

EXPORT(int, scalblnl) {
    return UNIMPLEMENTED();
}

EXPORT(int, scalbn) {
    return UNIMPLEMENTED();
}

EXPORT(int, scalbnf) {
    return UNIMPLEMENTED();
}

EXPORT(int, scalbnl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sin) {
    return UNIMPLEMENTED();
}

EXPORT(int, sinf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sinh) {
    return UNIMPLEMENTED();
}

EXPORT(int, sinhf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sinhl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sinl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sqrt) {
    return UNIMPLEMENTED();
}

EXPORT(int, sqrtf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sqrtl) {
    return UNIMPLEMENTED();
}

EXPORT(int, tan) {
    return UNIMPLEMENTED();
}

EXPORT(int, tanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, tanh) {
    return UNIMPLEMENTED();
}

EXPORT(int, tanhf) {
    return UNIMPLEMENTED();
}

EXPORT(int, tanhl) {
    return UNIMPLEMENTED();
}

EXPORT(int, tanl) {
    return UNIMPLEMENTED();
}

EXPORT(int, tgamma) {
    return UNIMPLEMENTED();
}

EXPORT(int, tgammaf) {
    return UNIMPLEMENTED();
}

EXPORT(int, tgammal) {
    return UNIMPLEMENTED();
}

EXPORT(int, trunc) {
    return UNIMPLEMENTED();
}

EXPORT(int, truncf) {
    return UNIMPLEMENTED();
}

EXPORT(int, truncl) {
    return UNIMPLEMENTED();
}
