//
//  File: %t-blank.c
//  Summary: "Blank datatype"
//  Section: datatypes
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Ren-C Open Source Contributors
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

#include "sys-core.h"


//
//  MF_Null: C
//
// Prior to generalized quoting, NULL did not have a rendering function and
// it was considered an error to try and mold them.  When quoting arrived,
// escaped NULL was renderable as its ticks, followed by nothing.  This is
// the "nothing" part, saving on a special-case for that.
//
void MF_Null(REB_MOLD *mo, REBCEL(const*) v, bool form)
{
    UNUSED(mo);
    UNUSED(form);
    UNUSED(v);
}


//
//  MF_Blank: C
//
void MF_Blank(REB_MOLD *mo, REBCEL(const*) v, bool form)
{
    UNUSED(form); // no distinction between MOLD and FORM
    UNUSED(v);
    Append_Ascii(mo->series, "_");
}


//
//  PD_Blank: C
//
// It is not possible to "poke" into a blank (and as an attempt at modifying
// operation, it is not swept under the rug).  But if picking with GET-PATH!
// or GET, we indicate no result with void.  (Ordinary path selection will
// treat this as an error.)
//
// This could also be taken care of with special code in path dispatch, but
// by putting it in a handler you only pay for the logic if you actually do
// encounter a blank.
//
REB_R PD_Blank(
    REBPVS *pvs,
    const RELVAL *picker,
    const REBVAL *opt_setval
){
    UNUSED(picker);
    UNUSED(pvs);

    if (opt_setval != NULL)
        return R_UNHANDLED;

    return nullptr;
}


//
//  CT_Blank: C
//
// Must have a comparison function, otherwise SORT would not work on arrays
// with blanks in them.
//
REBINT CT_Blank(REBCEL(const*) a, REBCEL(const*) b, bool strict)
{
    UNUSED(strict);  // no strict form of comparison
    UNUSED(a);
    UNUSED(b);

    return 0;  // All blanks are equal
}


//
//  REBTYPE: C
//
// While generics like SELECT are able to dispatch on BLANK! and return NULL,
// they do so by not running at all...see REB_TS_NOOP_IF_BLANK.
//
REBTYPE(Blank)
{
    switch (VAL_WORD_SYM(verb)) {
      case SYM_REFLECT: {
        INCLUDE_PARAMS_OF_REFLECT;
        UNUSED(ARG(value)); // taken care of by `unit` above.

        // !!! REFLECT cannot use REB_TS_NOOP_IF_BLANK, because of the special
        // case of TYPE OF...where a BLANK! in needs to provide BLANK! the
        // datatype out.  Also, there currently exist "reflectors" that
        // return LOGIC!, e.g. TAIL?...and logic cannot blindly return null:
        //
        // https://forum.rebol.info/t/954
        //
        // So for the moment, we just ad-hoc return nullptr for some that
        // R3-Alpha returned NONE! for.  Review.
        //
        switch (VAL_WORD_SYM(ARG(property))) {
          case SYM_INDEX:
          case SYM_LENGTH:
            return nullptr;

          default: break;
        }
        break; }

      case SYM_COPY: { // since `copy/deep [1 _ 2]` is legal, allow `copy _`
        INCLUDE_PARAMS_OF_COPY;
        UNUSED(ARG(value)); // already referenced as `unit`

        if (REF(part))
            fail (Error_Bad_Refines_Raw());

        UNUSED(REF(deep));
        UNUSED(REF(types));

        return Init_Blank(D_OUT); }

      default: break;
    }

    return R_UNHANDLED;
}



//
//  MF_Handle: C
//
void MF_Handle(REB_MOLD *mo, REBCEL(const*) v, bool form)
{
    UNUSED(form);  // !!! Handles have "no printable form", what to do here?
    UNUSED(v);

    Append_Ascii(mo->series, "#[handle!]");
}


//
//  CT_Handle: C
//
REBINT CT_Handle(REBCEL(const*) a, REBCEL(const*) b, bool strict)
{
    // Would it be meaningful to allow user code to compare HANDLE!?
    //
    UNUSED(a);
    UNUSED(b);
    UNUSED(strict);
    fail ("Currently comparing HANDLE! types is not allowed.");
}


//
// REBTYPE: C
//
// !!! Currently, in order to have a comparison function a datatype must also
// have a dispatcher for generics, and the comparison is essential.  Hence
// this cannot use a `-` in the %reb-types.r in lieu of this dummy function.
//
REBTYPE(Handle)
{
    UNUSED(frame_);
    UNUSED(verb);

    return R_UNHANDLED;
}
