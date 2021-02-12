//
//  File: %sys-nulled.h
//  Summary: "NULL definitions (transient evaluative cell--not a DATATYPE!)"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012-2020 Ren-C Open Source Contributors
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
// NULL is a transient evaluation product.  It is used as a signal for
// "soft failure", e.g. `find [a b] 'c` is NULL, hence they are conditionally
// false.  But null isn't an "ANY-VALUE!", and can't be stored in BLOCK!s that
// are seen by the user.
//
// The libRebol API takes advantage of this by actually using C's concept of
// a null pointer to directly represent the optional state.  By promising this
// is the case, clients of the API can write `if (value)` or `if (!value)`
// and be sure that there's not some nonzero address of a "null-valued cell".
// So there is no `isRebolNull()` API.
//
// But that's the API.  Internally, cells are the currency used, and if they
// are to represent an "optional" value, there must be a special bit pattern
// used to mark them as not containing any value at all.  These are called
// "nulled cells" and marked by means of their KIND3Q_BYTE().
//

#define NULLED_CELL \
    c_cast(const REBVAL*, &PG_Nulled_Cell)

#define IS_NULLED(v) \
    (VAL_TYPE(v) == REB_NULL)

inline static REBVAL *Init_Nulled_Core(RELVAL *out) {
    RESET_VAL_HEADER(out, REB_NULL, CELL_MASK_NONE);
  #ifdef ZERO_UNUSED_CELL_FIELDS
    EXTRA(Any, out).trash = nullptr;
    PAYLOAD(Any, out).first.trash = nullptr;
    PAYLOAD(Any, out).second.trash = nullptr;
  #endif
    return cast(REBVAL*, out);
}

#define Init_Nulled(out) \
    Init_Nulled_Core(TRACK_CELL_IF_DEBUG(out))

// This helps find callsites that are following the convention for what
// `do []` should do.  This has changed to be NULL from the historical choice
// to make it an "ornery" value (e.g. `~unset~`):
//
// https://forum.rebol.info/t/what-should-do-do/1426
//
#define Init_Empty_Nulled(out) \
    Init_Nulled(out)


//=//// NULL ISOTOPE (NULL-2) /////////////////////////////////////////////=//
//
// There was considerable deliberation about how to handle branches that
// actually want to return NULL without triggering ELSE:
//
//     >> if true [null] else [print "Don't want this to print"]
//     ; null (desired result)
//
// Making branch results NULL if-and-only-if the branch ran would mean having
// to distort the result (e.g. into a void).
//
// The ultimate solution to this was to introduce a slight variant of NULL
// which would be short-lived (e.g. "decay" to a normal NULL) but carry the
// additional information that it was an intended branch result.  While this
// seems sketchy, holistic survey of alternatives made it seem pretty much
// the best option in the bunch.  Everything else just shifted the same
// complexity around, in a way that would wind up burdening usages that were
// not involving ELSE or then whatsoever.
//
// The "decay" of NULL isotopes occurs on variable retrieval.  Hence:
//
//     >> x: if true [null]
//     ; null-2
//
//     >> x
//     ; null
//
// This means getting one's hands on a NULL isotope to start with is tricky,
// and has to be done with a function (NULL-2).
//
//     >> null-2
//     ; null-2
//
// As with the natural concept of radiation, working with NULL isotopes is
// risky, and should be avoided by code that doesn't need to do it.
//
// In order to avoid taking a relatively precious CELL_FLAG for this purpose,
// the isotope indication is done by making the HEART_BYTE() of the cell
// REB_BLANK, while keeping the surface byte REB_NULL.

inline static REBVAL *Init_Heavy_Nulled(RELVAL *out) {
    RESET_CELL(out, REB_NULL, CELL_MASK_NONE);
    mutable_HEART_BYTE(out) = REB_BLANK;
    return cast(REBVAL*, out);
}

inline static bool Is_Light_Nulled(const RELVAL *v)
  { return IS_NULLED(v) and HEART_BYTE(v) == REB_NULL; }

inline static bool Is_Heavy_Nulled(const RELVAL *v)
  { return IS_NULLED(v) and HEART_BYTE(v) == REB_BLANK; }

inline static RELVAL *Decay_If_Nulled(RELVAL *v) {
    if (IS_NULLED(v))  // cheaper to overwrite whether already REB_NULL or not
        mutable_HEART_BYTE(v) = REB_NULL;
    return v;
}

inline static RELVAL *Isotopify_If_Nulled(RELVAL *v) {
    if (IS_NULLED(v))  // cheaper to overwrite whether already REB_NULL or not
        mutable_HEART_BYTE(v) = REB_BLANK;
    return v;
}


// !!! A theory was that the "evaluated" flag would help a function that took
// both <opt> and <end>, which are converted to nulls, distinguish what kind
// of null it is.  This may or may not be a good idea, but unevaluating it
// here just to make a note of the concept, and tag it via the callsites.
//
#define Init_Endish_Nulled(out) \
    RESET_CELL((out), REB_NULL, CELL_FLAG_UNEVALUATED)

inline static bool IS_ENDISH_NULLED(const RELVAL *v)
    { return IS_NULLED(v) and GET_CELL_FLAG(v, UNEVALUATED); }

// To help ensure full nulled cells don't leak to the API, the variadic
// interface only accepts nullptr.  Any internal code with a REBVAL* that may
// be a "nulled cell" must translate any such cells to nullptr.
//
inline static const REBVAL *NULLIFY_NULLED(const REBVAL *cell)
  { return VAL_TYPE(cell) == REB_NULL ? nullptr : cell; }

inline static const REBVAL *REIFY_NULL(const REBVAL *cell)
  { return cell == nullptr ? NULLED_CELL : cell; }
