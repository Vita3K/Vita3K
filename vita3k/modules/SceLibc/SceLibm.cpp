// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include "SceLibm.h"

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

BRIDGE_IMPL(_Cosh)
BRIDGE_IMPL(_Dclass)
BRIDGE_IMPL(_Dsign)
BRIDGE_IMPL(_Dtest)
BRIDGE_IMPL(_Exp)
BRIDGE_IMPL(_FCosh)
BRIDGE_IMPL(_FDclass)
BRIDGE_IMPL(_FDsign)
BRIDGE_IMPL(_FDtest)
BRIDGE_IMPL(_FExp)
BRIDGE_IMPL(_FFpcomp)
BRIDGE_IMPL(_FLog)
BRIDGE_IMPL(_FSin)
BRIDGE_IMPL(_FSinh)
BRIDGE_IMPL(_FSinx)
BRIDGE_IMPL(_Fpcomp)
BRIDGE_IMPL(_LCosh)
BRIDGE_IMPL(_LDclass)
BRIDGE_IMPL(_LDsign)
BRIDGE_IMPL(_LDtest)
BRIDGE_IMPL(_LExp)
BRIDGE_IMPL(_LFpcomp)
BRIDGE_IMPL(_LLog)
BRIDGE_IMPL(_LSin)
BRIDGE_IMPL(_LSinh)
BRIDGE_IMPL(_LSinx)
BRIDGE_IMPL(_Log)
BRIDGE_IMPL(_Sin)
BRIDGE_IMPL(_Sinh)
BRIDGE_IMPL(_Sinx)
BRIDGE_IMPL(acos)
BRIDGE_IMPL(acosf)
BRIDGE_IMPL(acosh)
BRIDGE_IMPL(acoshf)
BRIDGE_IMPL(acoshl)
BRIDGE_IMPL(acosl)
BRIDGE_IMPL(asin)
BRIDGE_IMPL(asinf)
BRIDGE_IMPL(asinh)
BRIDGE_IMPL(asinhf)
BRIDGE_IMPL(asinhl)
BRIDGE_IMPL(asinl)
BRIDGE_IMPL(atan)
BRIDGE_IMPL(atan2)
BRIDGE_IMPL(atan2f)
BRIDGE_IMPL(atan2l)
BRIDGE_IMPL(atanf)
BRIDGE_IMPL(atanh)
BRIDGE_IMPL(atanhf)
BRIDGE_IMPL(atanhl)
BRIDGE_IMPL(atanl)
BRIDGE_IMPL(cbrt)
BRIDGE_IMPL(cbrtf)
BRIDGE_IMPL(cbrtl)
BRIDGE_IMPL(ceil)
BRIDGE_IMPL(ceilf)
BRIDGE_IMPL(ceill)
BRIDGE_IMPL(copysign)
BRIDGE_IMPL(copysignf)
BRIDGE_IMPL(copysignl)
BRIDGE_IMPL(cos)
BRIDGE_IMPL(cosf)
BRIDGE_IMPL(cosh)
BRIDGE_IMPL(coshf)
BRIDGE_IMPL(coshl)
BRIDGE_IMPL(cosl)
BRIDGE_IMPL(erf)
BRIDGE_IMPL(erfc)
BRIDGE_IMPL(erfcf)
BRIDGE_IMPL(erfcl)
BRIDGE_IMPL(erff)
BRIDGE_IMPL(erfl)
BRIDGE_IMPL(exp)
BRIDGE_IMPL(exp2)
BRIDGE_IMPL(exp2f)
BRIDGE_IMPL(exp2l)
BRIDGE_IMPL(expf)
BRIDGE_IMPL(expl)
BRIDGE_IMPL(expm1)
BRIDGE_IMPL(expm1f)
BRIDGE_IMPL(expm1l)
BRIDGE_IMPL(fabs)
BRIDGE_IMPL(fabsf)
BRIDGE_IMPL(fabsl)
BRIDGE_IMPL(fdim)
BRIDGE_IMPL(fdimf)
BRIDGE_IMPL(fdiml)
BRIDGE_IMPL(floor)
BRIDGE_IMPL(floorf)
BRIDGE_IMPL(floorl)
BRIDGE_IMPL(fma)
BRIDGE_IMPL(fmaf)
BRIDGE_IMPL(fmal)
BRIDGE_IMPL(fmax)
BRIDGE_IMPL(fmaxf)
BRIDGE_IMPL(fmaxl)
BRIDGE_IMPL(fmin)
BRIDGE_IMPL(fminf)
BRIDGE_IMPL(fminl)
BRIDGE_IMPL(fmod)
BRIDGE_IMPL(fmodf)
BRIDGE_IMPL(fmodl)
BRIDGE_IMPL(frexp)
BRIDGE_IMPL(frexpf)
BRIDGE_IMPL(frexpl)
BRIDGE_IMPL(hypot)
BRIDGE_IMPL(hypotf)
BRIDGE_IMPL(hypotl)
BRIDGE_IMPL(ilogb)
BRIDGE_IMPL(ilogbf)
BRIDGE_IMPL(ilogbl)
BRIDGE_IMPL(ldexp)
BRIDGE_IMPL(ldexpf)
BRIDGE_IMPL(ldexpl)
BRIDGE_IMPL(lgamma)
BRIDGE_IMPL(lgammaf)
BRIDGE_IMPL(lgammal)
BRIDGE_IMPL(llrint)
BRIDGE_IMPL(llrintf)
BRIDGE_IMPL(llrintl)
BRIDGE_IMPL(llround)
BRIDGE_IMPL(llroundf)
BRIDGE_IMPL(llroundl)
BRIDGE_IMPL(log)
BRIDGE_IMPL(log10)
BRIDGE_IMPL(log10f)
BRIDGE_IMPL(log10l)
BRIDGE_IMPL(log1p)
BRIDGE_IMPL(log1pf)
BRIDGE_IMPL(log1pl)
BRIDGE_IMPL(log2)
BRIDGE_IMPL(log2f)
BRIDGE_IMPL(log2l)
BRIDGE_IMPL(logb)
BRIDGE_IMPL(logbf)
BRIDGE_IMPL(logbl)
BRIDGE_IMPL(logf)
BRIDGE_IMPL(logl)
BRIDGE_IMPL(lrint)
BRIDGE_IMPL(lrintf)
BRIDGE_IMPL(lrintl)
BRIDGE_IMPL(lround)
BRIDGE_IMPL(lroundf)
BRIDGE_IMPL(lroundl)
BRIDGE_IMPL(modf)
BRIDGE_IMPL(modff)
BRIDGE_IMPL(modfl)
BRIDGE_IMPL(nan)
BRIDGE_IMPL(nanf)
BRIDGE_IMPL(nanl)
BRIDGE_IMPL(nearbyint)
BRIDGE_IMPL(nearbyintf)
BRIDGE_IMPL(nearbyintl)
BRIDGE_IMPL(nextafter)
BRIDGE_IMPL(nextafterf)
BRIDGE_IMPL(nextafterl)
BRIDGE_IMPL(nexttoward)
BRIDGE_IMPL(nexttowardf)
BRIDGE_IMPL(nexttowardl)
BRIDGE_IMPL(pow)
BRIDGE_IMPL(powf)
BRIDGE_IMPL(powl)
BRIDGE_IMPL(remainder)
BRIDGE_IMPL(remainderf)
BRIDGE_IMPL(remainderl)
BRIDGE_IMPL(remquo)
BRIDGE_IMPL(remquof)
BRIDGE_IMPL(remquol)
BRIDGE_IMPL(rint)
BRIDGE_IMPL(rintf)
BRIDGE_IMPL(rintl)
BRIDGE_IMPL(round)
BRIDGE_IMPL(roundf)
BRIDGE_IMPL(roundl)
BRIDGE_IMPL(scalbln)
BRIDGE_IMPL(scalblnf)
BRIDGE_IMPL(scalblnl)
BRIDGE_IMPL(scalbn)
BRIDGE_IMPL(scalbnf)
BRIDGE_IMPL(scalbnl)
BRIDGE_IMPL(sin)
BRIDGE_IMPL(sinf)
BRIDGE_IMPL(sinh)
BRIDGE_IMPL(sinhf)
BRIDGE_IMPL(sinhl)
BRIDGE_IMPL(sinl)
BRIDGE_IMPL(sqrt)
BRIDGE_IMPL(sqrtf)
BRIDGE_IMPL(sqrtl)
BRIDGE_IMPL(tan)
BRIDGE_IMPL(tanf)
BRIDGE_IMPL(tanh)
BRIDGE_IMPL(tanhf)
BRIDGE_IMPL(tanhl)
BRIDGE_IMPL(tanl)
BRIDGE_IMPL(tgamma)
BRIDGE_IMPL(tgammaf)
BRIDGE_IMPL(tgammal)
BRIDGE_IMPL(trunc)
BRIDGE_IMPL(truncf)
BRIDGE_IMPL(truncl)
