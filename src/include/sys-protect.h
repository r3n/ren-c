//
//  File: %sys-protect.h
//  Summary: "System Const and Protection Functions"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2018 Rebol Open Source Contributors
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
// R3-Alpha introduced the idea of "protected" series and variables.  Ren-C
// introduces a new form of read-only-ness that is not a bit on series, but
// rather bits on values.  This means that a value can be a read-only view of
// a series that is otherwise mutable.
//
// !!! Checking for read access was a somewhat half-baked feature in R3-Alpha,
// as heeding the protection bit had to be checked explicitly.  Many places in
// the code did not do the check.  While several bugs of that nature have
// been replaced in an ad-hoc fashion, a better solution would involve using
// C's `const` feature to locate points that needed to promote series access
// to be mutable, so it could be checked at compile-time.
//


// Flags used for Protect functions
//
enum {
    PROT_SET = 1 << 0,
    PROT_DEEP = 1 << 1,
    PROT_HIDE = 1 << 2,
    PROT_WORD = 1 << 3,
    PROT_FREEZE = 1 << 4
};

inline static bool Is_Array_Frozen_Shallow(REBARR *a)
  { return GET_SERIES_INFO(a, FROZEN_SHALLOW); }

inline static bool Is_Array_Frozen_Deep(REBARR *a) {
    if (NOT_SERIES_INFO(a, FROZEN_DEEP))
        return false;

    assert(GET_SERIES_INFO(a, FROZEN_SHALLOW));  // implied by FROZEN_DEEP
    return true;
}

inline static REBARR *Freeze_Array_Deep(REBARR *a) {
    Protect_Series(
        SER(a),
        0, // start protection at index 0
        PROT_DEEP | PROT_SET | PROT_FREEZE
    );
    Uncolor_Array(a);
    return a;
}

inline static REBARR *Freeze_Array_Shallow(REBARR *a) {
    SET_SERIES_INFO(a, FROZEN_SHALLOW);
    return a;
}

#define Is_Array_Shallow_Read_Only(a) \
    Is_Series_Read_Only(SER(a))

#define Force_Value_Frozen_Deep(v) \
    Force_Value_Frozen_Core((v), true, SER(EMPTY_ARRAY))  // auto-locked

#define Force_Value_Frozen_Deep_Blame(v,blame) \
    Force_Value_Frozen_Core((v), true, SER(blame))

#define Force_Value_Frozen_Shallow(v) \
    Force_Value_Frozen_Core((v), false, SER(EMPTY_ARRAY))  // auto-locked

