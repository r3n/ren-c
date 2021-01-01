//
//  File: %sys-blank.h
//  Summary: "BLANK! Datatype Header"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2019 Ren-C Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Lesser GPL, Version 3.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.gnu.org/licenses/lgpl-3.0.html
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Blank! values are a kind of "reified" null, and you can convert between
// them using TRY and OPT:
//
//     >> try ()
//     == _
//
//     >> opt _
//     ; null
//
// Like null, they are considered to be false--like the LOGIC! #[false] value.
// Only these three things are conditionally false in Rebol, and testing for
// conditional truth and falsehood is frequent.  Hence in addition to its
// type, BLANK! also carries a header bit that can be checked for conditional
// falsehood, to save on needing to separately test the type.
//

#define BLANK_VALUE \
    c_cast(const REBVAL*, &PG_Blank_Value)

inline static REBVAL *Init_Blank_Core(RELVAL *v) {
    RESET_VAL_HEADER(v, REB_BLANK, CELL_MASK_NONE);
  #ifdef ZERO_UNUSED_CELL_FIELDS
    EXTRA(Any, v).trash = nullptr;
    PAYLOAD(Any, v).first.trash = nullptr;
    PAYLOAD(Any, v).second.trash = nullptr;
  #endif
    return cast(REBVAL*, v);
}

#define Init_Blank(v) \
    Init_Blank_Core(TRACK_CELL_IF_DEBUG(v))
