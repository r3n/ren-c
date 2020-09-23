//
//  File: %f-round.c
//  Summary: "special rounding math functions"
//  Section: functional
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
//=////////////////////////////////////////////////////////////////////////=//
//

#include "sys-core.h"

#include "datatypes/sys-money.h"


#define Dec_Trunc(x) (((x) < 0.0) ? -1.0 : 1.0) * floor(fabs(x))
#define Dec_Away(x) (((x) < 0.0) ? -1.0 : 1.0) * ceil(fabs(x))

//
//  Round_Dec: C
//
// Identical to ROUND mezzanine function.
// Note: scale arg only valid if RF_TO is set
//
REBDEC Round_Dec(REBDEC dec, REBFRM *frame_, REBDEC scale)
{
    INCLUDE_PARAMS_OF_ROUND;
    UNUSED(ARG(value));  // was extracted for `dec`

    REBDEC r;
    union {REBDEC d; REBI64 i;} m;
    REBI64 j;

    if (REF(to)) {
        if (scale == 0.0)
            fail (Error_Zero_Divide_Raw());
        scale = fabs(scale);
    } else
        scale = 1.0;

    if (scale < ldexp(fabs(dec), -53))
        return dec; // scale is "negligible"

    bool v = (scale >= 1.0);

    int e;
    if (v) {
        dec = dec / scale;
        UNUSED(e);
        e = -1020; // suppress compiler warning
    }
    else {
        r = frexp(scale, &e);
        if (e <= -1022) {
            scale = r;
            dec = ldexp(dec, e);
        } else
            e = 0;
        scale = 1.0 / scale;
        dec = dec * scale;
    }
    if (REF(down) or REF(floor) or REF(ceiling)) {
        if (REF(floor)) dec = floor(dec);
        else if (REF(down)) dec = Dec_Trunc(dec);
        else dec = ceil(dec);
    } else {
        /*  integer-compare fabs(dec) and floor(fabs(dec)) + 0.5,
            which is equivalent to "tolerant comparison" of the
            fractional part with 0.5                                */
        m.d = fabs(dec);
        j = m.i;
        m.d = floor(m.d) + 0.5;
        if (j - m.i < -10) dec = Dec_Trunc(dec);
        else if (j - m.i > 10) dec = Dec_Away(dec);
        else if (REF(even)) {
            if (fmod(fabs(dec), 2.0) < 1.0) dec = Dec_Trunc(dec);
            else dec = Dec_Away(dec);
        }
        else if (REF(half_down)) dec = Dec_Trunc(dec);
        else if (REF(half_ceiling)) dec = ceil(dec);
        else dec = Dec_Away(dec);
    }

    if (v) {
        if (fabs(dec = dec * scale) != HUGE_VAL)
            return dec;
        else
            fail (Error_Overflow_Raw());
    }
    return ldexp(dec / scale, e);
}

#define Int_Abs(x) ((x) < 0) ? -(x) : (x)

#define Int_Trunc { \
    num = (num > 0) ? cast(REBI64, n - r) : -cast(REBI64, n - r); \
}

#define Int_Floor { \
    if (num > 0) \
        num = n - r; \
    else if ((m = n + s) <= cast(REBU64, 1) << 63) \
        num = -cast(REBI64, m); \
    else \
        fail (Error_Overflow_Raw()); \
}

#define Int_Ceil { \
    if (num < 0) \
        num = -cast(REBI64, n - r); \
    else if ((m = n + s) < cast(REBU64, 1) << 63) \
        num = m; \
    else \
        fail (Error_Overflow_Raw()); \
}

#define Int_Away { \
    if ((m = n + s) >= cast(REBU64, 1) << 63) \
        if (num < 0 && m == cast(REBU64, 1) << 63) \
            num = m; \
        else \
            fail (Error_Overflow_Raw()); \
    else \
        num = (num > 0) ? cast(REBI64, m) : -cast(REBI64, m); \
}


//
//  Round_Int: C
//
// Identical to ROUND mezzanine function.
// Note: scale arg only valid if RF_TO is set
//
REBI64 Round_Int(REBI64 num, REBFRM *frame_, REBI64 scale)
{
    INCLUDE_PARAMS_OF_ROUND;
    UNUSED(ARG(value));  // was extracted as `num`

    /* using safe unsigned arithmetic */
    REBU64 sc, n, r, m, s;

    if (REF(to)) {
        if (scale == 0) fail (Error_Zero_Divide_Raw());
        sc = Int_Abs(scale);
    }
    else sc = 1;

    n = Int_Abs(num);
    r = n % sc;
    s = sc - r;
    if (r == 0) return num;

    if (REF(down) or REF(floor) or REF(ceiling)) {
        if (REF(down)) {Int_Trunc; return num;}
        if (REF(floor)) {Int_Floor; return num;}
        Int_Ceil;
        return num;
    }

    /* "genuine" rounding */
    if (r < s) {Int_Trunc; return num;}
    else if (r > s) {Int_Away; return num;}

    /* half */
    if (REF(even)) {
        if ((n / sc) & 1) {Int_Away; return num;}
        else {Int_Trunc; return num;}
    }
    if (REF(half_down)) {Int_Trunc; return num;}
    if (REF(half_ceiling)) {Int_Ceil; return num;}

    Int_Away; return num; /* this is round_half_away */
}

//
//  Round_Deci: C
//
// Identical to ROUND mezzanine function.
// Note: scale arg only valid if REF(to) is set
//
deci Round_Deci(deci num, REBFRM *frame_, deci scale)
{
    INCLUDE_PARAMS_OF_ROUND;
    UNUSED(ARG(value));  // was extracted as `num`

    deci deci_one = {1u, 0u, 0u, 0u, 0};

    if (REF(to)) {
        if (deci_is_zero(scale)) fail (Error_Zero_Divide_Raw());
        scale = deci_abs(scale);
    }
    else scale = deci_one;

    if (REF(even)) return deci_half_even(num, scale);
    if (REF(down)) return deci_truncate(num, scale);
    if (REF(half_down)) return deci_half_truncate(num, scale);
    if (REF(floor)) return deci_floor(num, scale);
    if (REF(ceiling)) return deci_ceil(num, scale);
    if (REF(half_ceiling)) return deci_half_ceil(num, scale);

    return deci_half_away(num, scale);
}
