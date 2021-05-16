//
//  File: %sys-void.h
//  Summary: "BAD-WORD! Datatype Header"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2021 Ren-C Open Source Contributors
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
// "BAD-WORD! is a means of giving a hot potato back that is a warning of
//  something, but you don't want to force an error 'in the moment'...
//  in case the returned information wasn't going to be used anyway."
//
// https://forum.rebol.info/t/947
//
// BAD-WORD!s are the default return values for things like `do []`, or
// `print "Hello"`.  Unlike NULL, a bad word *is* a value...however a somewhat
// unfriendly one.  While NULLs are falsey, bad words are *neither* truthy nor
// falsey.  Though a bad-word! can be put in an array (a NULL can't), the
// baseline access through a word! or path! access will error.
//
//=//// NOTES //////////////////////////////////////////////////////////////=//
//
// * See %sys-trash.h for a special case of a cell that will trigger panics
//   if it is ever read in the debug build, but is just an ordinary BAD-WORD!
//   of ~trash~ in the release build.
//

inline static REBVAL *Init_Bad_Word_Untracked(
    RELVAL *out,
    const REBSTR *label,
    REBFLGS flags
){
    RESET_VAL_HEADER(out, REB_BAD_WORD, CELL_FLAG_FIRST_IS_NODE | flags);

    // Due to being evaluator active and not wanting to disrupt the order in
    // %types.r, voids claim to be bindable...and just set the binding to null.
    // See %sys-ordered.h for more on all the rules that make this so.
    //
    mutable_BINDING(out) = nullptr;

    INIT_VAL_NODE1(out, label);
  #ifdef ZERO_UNUSED_CELL_FIELDS
    PAYLOAD(Any, out).second.trash = nullptr;
  #endif
    return cast(REBVAL*, out);
}

#define Init_Bad_Word_Core(out,label,flags) \
    Init_Bad_Word_Untracked(TRACK_CELL_IF_DEBUG(out), (label), (flags))

inline static const REBSYM* VAL_BAD_WORD_LABEL(
    REBCEL(const*) v
){
    assert(CELL_KIND(v) == REB_BAD_WORD);
    assert(GET_CELL_FLAG(v, FIRST_IS_NODE));
    return cast(const REBSYM*, VAL_NODE1(v));
}

#define VAL_BAD_WORD_ID(v) \
    ID_OF_SYMBOL(VAL_BAD_WORD_LABEL(v))


//=//// CURSE WORDS ////////////////////////////////////////////////////////=//

// A "curse word" is when a BAD-WORD! does not have the friendly bit set (e.g.
// it has been evaluated, and is not being manipulated as raw material)

inline static bool Is_Curse_Word(const RELVAL *v, enum Reb_Symbol_Id sym) {
    assert(sym != SYM_0);
    if (not IS_BAD_WORD(v))
        return false;
    if (GET_CELL_FLAG(v, ISOTOPE))
        return false;  // friendly form of BAD-WORD!
    if (cast(REBLEN, sym) == cast(REBLEN, VAL_BAD_WORD_ID(v)))
        return true;
    return false;
}

#define Init_Curse_Word(out,sym) \
    Init_Bad_Word_Core((out), Canon(sym), CELL_MASK_NONE)


// ~unset~ is chosen in particular by the system to represent variables that
// have not been  assigned.

#define UNSET_VALUE         c_cast(const REBVAL*, &PG_Unset_Value)
#define Init_Unset(out)     Init_Curse_Word((out), SYM_UNSET)
#define Is_Unset(v)         Is_Curse_Word(v, SYM_UNSET)


// `~void~` is treated specially by the system, to convey "invisible intent".
// It is what `do []` evaluates to, as well as `do [comment "hi"])`.
//
// This is hidden by the console, though perhaps there could be better ideas
// (like printing `; == ~void~` if the command you ran had no other output
// printed, just so you know it wasn't a no-op?)

#define Init_Void(out)      Init_Curse_Word((out), SYM_VOID)
#define Is_Void(v)          Is_Curse_Word((v), SYM_VOID)


// See EVAL_FLAG_INPUT_WAS_INVISIBLE for the rationale behind ~stale~, that
// has a special relationship with ~void~.
//
#define Init_Stale(out)     Init_Curse_Word((out), SYM_STALE)
#define Is_Stale(v)         Is_Curse_Word((v), SYM_STALE)


// `~none~` is the default RETURN for when you just write something like
// `func [return: []] [...]`.  It represents the intention of not having a
// return value, but reserving the right to not be treated as invisible, so
// that if one ever did imagine an interesting value for it to return, the
// callsites wouldn't have assumed it was invisible.
//
// Even a function like PRINT has a potentially interesting return value,
// given that it channels through NULL if the print content vaporized and
// it printed nothing (not even a newline).  This lets you use it with ELSE,
// and you couldn't write `print [...] else [...]` if it would be sometimes
// invisible and sometimes not.
//
#define Init_None(out)      Init_Curse_Word((out), SYM_NONE)
#define Is_None(v)          Is_Curse_Word((v), SYM_NONE)


//=//// NULL ISOTOPE (unfriendly ~null~) ///////////////////////////////////=//
//
// There was considerable deliberation about how to handle branches that
// actually want to return NULL without triggering ELSE:
//
//     >> if true [null] else [print "Don't want this to print"]
//     ; null (desired result)
//
// Making branch results NULL if-and-only-if the branch ran would mean having
// to distort the result.
//
// The ultimate solution to this was to introduce a slight variant of NULL
// which would be short-lived (e.g. "decay" to a normal NULL) but carry the
// additional information that it was an intended branch result.  This
// seemed sketchy at first, but with ^(...) acting as a "detector" for those
// who need to know the difference, it has become a holistic solution.
//
// The "decay" of NULL isotopes occurs on variable retrieval.  Hence:
//
//     >> x: if true [null]
//     == ~null~  ; isotope
//
//     >> x
//     ; null
//
// As with the natural concept of radiation, working with NULL isotopes can
// be tricky, and should be avoided by code that doesn't need to do it.  (But
// it has actually gotten much easier with ^(...) behaviors.)
//

inline static REBVAL *Init_Heavy_Nulled(RELVAL *out) {
    Init_Curse_Word(out, SYM_NULL);
    return cast(REBVAL*, out);
}

inline static bool Is_Light_Nulled(const RELVAL *v)
  { return IS_NULLED(v); }

inline static bool Is_Heavy_Nulled(const RELVAL *v)
  { return Is_Curse_Word(v, SYM_NULL); }

inline static RELVAL *Decay_If_Nulled(RELVAL *v) {
    if (Is_Curse_Word(v, SYM_NULL))
        Init_Nulled(v);
    return v;
}

inline static RELVAL *Isotopify_If_Nulled(RELVAL *v) {
    if (IS_NULLED(v))
        Init_Heavy_Nulled(v);
    return v;
}

// When a parameter is "normal" then it is willing to turn the unfriendly form
// of ~null~ into a regular null.  This is leveraged by the API in order to
// make some common forms of null handling work more smoothly.

inline static REBVAL *Normalize(REBVAL *v) {
    Decay_If_Nulled(v);
    return v;
}


//=//// CELL MOVEMENT //////////////////////////////////////////////////////=//

// Moving a cell invalidates the old location.  This idea is a potential
// prelude to being able to do some sort of reference counting on series based
// on the cells that refer to them tracking when they are overwritten.  In
// the meantime, setting to unreadable void helps see when a value that isn't
// thought to be used any more is still being used.
//
// (It basically would involve setting the old cell to trash, so the functions
// live here for now.)

inline static REBVAL *Move_Cell_Untracked(
    RELVAL *out,
    REBVAL *v,
    REBFLGS copy_mask
){
    Copy_Cell_Core(out, v, copy_mask);
  #if defined(NDEBUG)
    Init_Trash(v);  // no advantage in release build (yet!)
  #endif
    return cast(REBVAL*, out);
}

#define Move_Cell(out,v) \
    Move_Cell_Untracked(TRACK_CELL_IF_DEBUG(out), (v), CELL_MASK_COPY)

#define Move_Cell_Core(out,v,cell_mask) \
    Move_Cell_Untracked(TRACK_CELL_IF_DEBUG(out), (v), (cell_mask))
