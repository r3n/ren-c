//
//  File: %f-series.c
//  Summary: "common series handling functions"
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

#define THE_SIGN(v) ((v < 0) ? -1 : (v > 0) ? 1 : 0)

//
//  Series_Common_Action_Maybe_Unhandled: C
//
// This routine is called to handle actions on ANY-SERIES! that can be taken
// care of without knowing what specific kind of series it is.  So generally
// index manipulation, and things like LENGTH/etc.
//
// It only works when the operation in question applies to an understanding of
// a series as containing fixed-size units.
//
REB_R Series_Common_Action_Maybe_Unhandled(
    REBFRM *frame_,
    const REBVAL *verb
){
    REBVAL *value = D_ARG(1);

    REBINT index = cast(REBINT, VAL_INDEX(value));
    REBINT tail = cast(REBINT, VAL_LEN_HEAD(value));

    REBFLGS sop_flags;  // "SOP_XXX" Set Operation Flags

    switch (VAL_WORD_SYM(verb)) {
      case SYM_REFLECT: {
        INCLUDE_PARAMS_OF_REFLECT;
        UNUSED(PAR(value));  // covered by `value`

        REBSYM property = VAL_WORD_SYM(ARG(property));
        assert(property != SYM_0);

        switch (property) {
          case SYM_INDEX:
            return Init_Integer(D_OUT, cast(REBI64, index) + 1);

          case SYM_LENGTH:
            return Init_Integer(D_OUT, tail > index ? tail - index : 0);

          case SYM_HEAD:
            Move_Value(D_OUT, value);
            VAL_INDEX(D_OUT) = 0;
            return Trust_Const(D_OUT);

          case SYM_TAIL:
            Move_Value(D_OUT, value);
            VAL_INDEX(D_OUT) = cast(REBLEN, tail);
            return Trust_Const(D_OUT);

          case SYM_HEAD_Q:
            return Init_Logic(D_OUT, index == 0);

          case SYM_TAIL_Q:
            return Init_Logic(D_OUT, index >= tail);

          case SYM_PAST_Q:
            return Init_Logic(D_OUT, index > tail);

          case SYM_FILE: {
            REBSER *s = VAL_SERIES(value);
            if (not IS_SER_ARRAY(s))
                return nullptr;
            if (NOT_ARRAY_FLAG(s, HAS_FILE_LINE_UNMASKED))
                return nullptr;
            return Init_File(D_OUT, LINK_FILE(s)); }

          case SYM_LINE: {
            REBSER *s = VAL_SERIES(value);
            if (not IS_SER_ARRAY(s))
                return nullptr;
            if (NOT_ARRAY_FLAG(s, HAS_FILE_LINE_UNMASKED))
                return nullptr;
            return Init_Integer(D_OUT, MISC(s).line); }

          default:
            break;
        }

        break; }

      case SYM_SKIP:
      case SYM_AT: {
        INCLUDE_PARAMS_OF_SKIP; // must be compatible with AT
        UNUSED(ARG(series)); // is already `value`

        REBVAL *offset = ARG(offset);

        REBINT len = Get_Num_From_Arg(offset);
        REBI64 i;
        if (VAL_WORD_SYM(verb) == SYM_SKIP) {
            //
            // `skip x logic` means `either logic [skip x] [x]` (this is
            // reversed from R3-Alpha and Rebol2, which skipped when false)
            //
            if (IS_LOGIC(offset)) {
                if (VAL_LOGIC(offset))
                    i = cast(REBI64, index) + 1;
                else
                    i = cast(REBI64, index);
            }
            else {
                // `skip series 1` means second element, add the len as-is
                //
                i = cast(REBI64, index) + cast(REBI64, len);
            }
        }
        else {
            assert(VAL_WORD_SYM(verb) == SYM_AT);

            // `at series 1` means first element, adjust index
            //
            // !!! R3-Alpha did this differently for values > 0 vs not, is
            // this what's intended?
            //
            if (len > 0)
                i = cast(REBI64, index) + cast(REBI64, len) - 1;
            else
                i = cast(REBI64, index) + cast(REBI64, len);
        }

        if (i > cast(REBI64, tail)) {
            if (REF(only))
                return nullptr;
            i = cast(REBI64, tail); // past tail clips to tail if not /ONLY
        }
        else if (i < 0) {
            if (REF(only))
                return nullptr;
            i = 0; // past head clips to head if not /ONLY
        }

        VAL_INDEX(value) = cast(REBLEN, i);
        RETURN (Trust_Const(value)); }

      case SYM_REMOVE: {
        INCLUDE_PARAMS_OF_REMOVE;
        UNUSED(PAR(series));  // accounted for by `value`

        FAIL_IF_READ_ONLY(value);

        REBINT len;
        if (REF(part))
            len = Part_Len_May_Modify_Index(value, ARG(part));
        else
            len = 1;

        index = cast(REBINT, VAL_INDEX(value));
        if (index < tail and len != 0)
            Remove_Any_Series_Len(value, VAL_INDEX(value), len);

        RETURN (value); }

      case SYM_INTERSECT:
        sop_flags = SOP_FLAG_CHECK;
        goto set_operation;

      case SYM_UNION:
        sop_flags = SOP_FLAG_BOTH;
        goto set_operation;

      case SYM_DIFFERENCE:
        sop_flags = SOP_FLAG_BOTH | SOP_FLAG_CHECK | SOP_FLAG_INVERT;
        goto set_operation;

      set_operation: {
        INCLUDE_PARAMS_OF_DIFFERENCE;  // should all have same spec
        UNUSED(ARG(value1));  // covered by `value`

        if (IS_BINARY(value))
            return R_UNHANDLED; // !!! unhandled; use bitwise math, for now

        return Init_Any_Series(
            D_OUT,
            VAL_TYPE(value),
            Make_Set_Operation_Series(
                value,
                ARG(value2),
                sop_flags,
                did REF(case),
                REF(skip) ? Int32s(ARG(skip), 1) : 1
            )
        ); }

      default:
        break;
    }

    return R_UNHANDLED; // not a common operation, uhandled (not NULLED_CELL!)
}


//
//  Cmp_Array: C
//
// Compare two arrays and return the difference of the first
// non-matching value.
//
REBINT Cmp_Array(const REBCEL *sval, const REBCEL *tval, bool is_case)
{
    if (C_STACK_OVERFLOWING(&is_case))
        Fail_Stack_Overflow();

    if (
        VAL_SERIES(sval) == VAL_SERIES(tval)
        and VAL_INDEX(sval) == VAL_INDEX(tval)
    ){
         return 0;
    }

    RELVAL *s = VAL_ARRAY_AT(sval);
    RELVAL *t = VAL_ARRAY_AT(tval);

    if (IS_END(s) or IS_END(t))
        goto diff_of_ends;

    while (
        VAL_TYPE(s) == VAL_TYPE(t)
        or (ANY_NUMBER(s) and ANY_NUMBER(t))
    ){
        REBINT diff;
        if ((diff = Cmp_Value(s, t, is_case)) != 0)
            return diff;

        s++;
        t++;

        if (IS_END(s) or IS_END(t))
            goto diff_of_ends;
    }

    return VAL_TYPE(s) - VAL_TYPE(t);

diff_of_ends:
    // Treat end as if it were a REB_xxx type of 0, so all other types would
    // compare larger than it.
    //
    if (IS_END(s)) {
        if (IS_END(t))
            return 0;
        return -1;
    }
    return 1;
}


//
//  Cmp_Value: C
//
// Compare two values and return the difference.
//
// is_case should be true for case sensitive compare
//
REBINT Cmp_Value(const RELVAL *sval, const RELVAL *tval, bool is_case)
{
    if (is_case and (VAL_NUM_QUOTES(sval) != VAL_NUM_QUOTES(tval)))
        return VAL_NUM_QUOTES(sval) - VAL_NUM_QUOTES(tval);

    const REBCEL *s = VAL_UNESCAPED(sval);
    const REBCEL *t = VAL_UNESCAPED(tval);
    enum Reb_Kind s_kind = CELL_KIND(s);
    enum Reb_Kind t_kind = CELL_KIND(t);

    if (
        s_kind != t_kind
        and not (ANY_NUMBER_KIND(s_kind) and ANY_NUMBER_KIND(t_kind))
    ){
        return s_kind - t_kind;
    }

    // !!! The strange and ad-hoc way this routine was written has some
    // special-case handling for numeric types.  It only allows the values to
    // be of unequal types below if they are both ANY-NUMBER!, so those cases
    // are more complex and jump around, reusing code via a goto and passing
    // the canonized decimal form via d1/d2.
    //
    REBDEC d1;
    REBDEC d2;

    switch (s_kind) {
      case REB_INTEGER:
        if (t_kind == REB_DECIMAL) {
            d1 = cast(REBDEC, VAL_INT64(s));
            d2 = VAL_DECIMAL(t);
            goto chkDecimal;
        }
        return THE_SIGN(VAL_INT64(s) - VAL_INT64(t));

      case REB_LOGIC:
        return VAL_LOGIC(s) - VAL_LOGIC(t);

      case REB_CHAR: { // REBUNI is unsigned, use compares vs. cast+THE_SIGN
        if (is_case) {
            if (VAL_CHAR(s) > VAL_CHAR(t))
                return 1;
            if (VAL_CHAR(s) < VAL_CHAR(t))
                return -1;
            return 0;
        }

        if (UP_CASE(VAL_CHAR(s)) > UP_CASE(VAL_CHAR(t)))
            return 1;
        if (UP_CASE(VAL_CHAR(s)) < UP_CASE(VAL_CHAR(t)))
            return -1;
        return 0; }

      case REB_PERCENT:
      case REB_DECIMAL:
      case REB_MONEY:
        if (s_kind == REB_MONEY)
            d1 = deci_to_decimal(VAL_MONEY_AMOUNT(s));
        else
            d1 = VAL_DECIMAL(s);
        if (t_kind == REB_INTEGER)
            d2 = cast(REBDEC, VAL_INT64(t));
        else if (t_kind == REB_MONEY)
            d2 = deci_to_decimal(VAL_MONEY_AMOUNT(t));
        else
            d2 = VAL_DECIMAL(t);

      chkDecimal:;

        if (Eq_Decimal(d1, d2))
            return 0;
        if (d1 < d2)
            return -1;
        return 1;

      case REB_PAIR:
        return Cmp_Pair(s, t);

      case REB_TUPLE:
        return Cmp_Tuple(s, t);

      case REB_TIME:
        return Cmp_Time(s, t);

      case REB_DATE:
        return Cmp_Date(s, t);

      case REB_BLOCK:
      case REB_SET_BLOCK:
      case REB_GET_BLOCK:
      case REB_SYM_BLOCK:
      case REB_GROUP:
      case REB_SET_GROUP:
      case REB_GET_GROUP:
      case REB_SYM_GROUP:
      case REB_PATH:
      case REB_SET_PATH:
      case REB_GET_PATH:
      case REB_SYM_PATH:
        return Cmp_Array(s, t, is_case);

      case REB_MAP:
        return Cmp_Array(s, t, is_case);  // !!! Fails if wrong hash size (!)

      case REB_TEXT:
      case REB_FILE:
      case REB_EMAIL:
      case REB_URL:
      case REB_TAG:
      case REB_ISSUE:
        return Compare_String_Vals(s, t, not is_case);

      case REB_BITSET: {  // !!! Temporarily init as binaries at index 0
        DECLARE_LOCAL (stemp);
        DECLARE_LOCAL (ttemp);
        Init_Binary(stemp, VAL_BITSET(s));
        Init_Binary(ttemp, VAL_BITSET(t));
        return Compare_Binary_Vals(stemp, ttemp); }

      case REB_BINARY:
        return Compare_Binary_Vals(s, t);

      case REB_DATATYPE:
        return VAL_TYPE_KIND(s) - VAL_TYPE_KIND(t);

      case REB_WORD:
      case REB_SET_WORD:
      case REB_GET_WORD:
      case REB_SYM_WORD:
        return Compare_Word(s,t,is_case);

      case REB_ERROR:
      case REB_OBJECT:
      case REB_MODULE:
      case REB_PORT:
        return VAL_CONTEXT(s) - VAL_CONTEXT(t);

      case REB_ACTION:
        return VAL_ACT_PARAMLIST(s) - VAL_ACT_PARAMLIST(t);

      case REB_CUSTOM:
        //
        // !!! Comparison in R3-Alpha never had a design document; it's not
        // clear what all the variations were for.  Extensions have a CT_XXX
        // hook, what's different about that from the Cmp_XXX functions?
        //
        /* return Cmp_Gob(s, t); */
        /* return Compare_Vector(s, t); */
        /* return Cmp_Struct(s, t); */
        /* return Cmp_Event(s, t); */
        /* return VAL_LIBRARY(s) - VAL_LIBRARY(t); */
        fail ("Temporary disablement of CUSTOM! comparisons");

      case REB_BLANK:
      case REB_NULLED: // !!! should nulls be allowed at this level?
      case REB_VOID:
        break;

      default:
        panic (nullptr); // all cases should be handled above
    }
    return 0;
}


//
//  Find_In_Array_Simple: C
//
// Simple search for a value in an array. Return the index of
// the value or the TAIL index if not found.
//
REBLEN Find_In_Array_Simple(REBARR *array, REBLEN index, const RELVAL *target)
{
    RELVAL *value = ARR_HEAD(array);

    for (; index < ARR_LEN(array); index++) {
        if (0 == Cmp_Value(value + index, target, false))
            return index;
    }

    return ARR_LEN(array);
}
