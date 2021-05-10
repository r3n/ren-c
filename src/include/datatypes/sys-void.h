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
// BAD-WORD!s are the default retur values for things like `do []`, or
// `print "Hello"`.  Unlike NULL, a bad word *is* a value...however a somewhat
// unfriendly one.  While NULLs are falsey, bad words are *neither* truthy nor
// falsey.  Though a bad-word! can be put in an array (a NULL can't), the
// baseline access through a word! or path! access will error.
//
// In the debug build, it is possible to make an "unreadable" bad-word!.  This
// will behave neutrally as far as the garbage collector is concerned, so
// it can be used as a placeholder for a value that will be filled in at
// some later time--spanning an evaluation.  But if the special IS_UNREADABLE
// checks are not used, it will not respond to IS_BAD_WORD() and will also
// refuse VAL_TYPE() checks.  This is useful anytime a placeholder is needed
// in a slot temporarily where the code knows it's supposed to come back and
// fill in the correct thing later...where the asserts serve as a reminder
// if that fill in never happens.
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

#define Init_Bad_Word(out,sym) \
    Init_Bad_Word_Core((out), Canon(sym), CELL_MASK_NONE)


// `~void~` is treated specially by the system, to convey "invisible intent".
// It is what `do []` evaluates to, as well as `do [comment "hi"])`.
//
// This is hidden by the console, though perhaps there could be better ideas
// (like printing `; == ~void~` if the command you ran had no other output
// printed, just so you know it wasn't a no-op?)

#define Init_Void(out) \
    Init_Bad_Word((out), SYM_VOID)


// See EVAL_FLAG_INPUT_WAS_INVISIBLE for the rationale behind ~stale~, that
// has a special relationship with ~void~.
//
#define Init_Stale(out) \
    Init_Bad_Word((out), SYM_STALE)


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
#define Init_None(out) \
    Init_Bad_Word((out), SYM_NONE)


inline static const REBSYM* VAL_BAD_WORD_LABEL(
    REBCEL(const*) v
){
    assert(CELL_KIND(v) == REB_BAD_WORD);
    assert(GET_CELL_FLAG(v, FIRST_IS_NODE));
    return cast(const REBSYM*, VAL_NODE1(v));
}

inline static bool Is_Bad_Word_With_Sym(const RELVAL *v, SYMID sym) {
    assert(sym != SYM_0);
    if (not IS_BAD_WORD(v))
        return false;
    if (cast(REBLEN, sym) == cast(REBLEN, ID_OF_SYMBOL(VAL_BAD_WORD_LABEL(v))))
        return true;
    return false;
}


#if !defined(DEBUG_UNREADABLE_VOIDS)  // release behavior, just ~unreadable~
    #define Init_Unreadable(v) \
        Init_Bad_Word_Core((v), PG_Unreadable_Canon, CELL_MASK_NONE)

    #define IS_BAD_WORD_RAW(v) \
        IS_BAD_WORD(v)

    #define IS_UNREADABLE_DEBUG(v) false

    #define ASSERT_UNREADABLE_IF_DEBUG(v) \
        assert(IS_BAD_WORD(v))  // would have to be a void even if not unreadable

    #define ASSERT_READABLE_IF_DEBUG(v) \
        NOOP
#else
    inline static REBVAL *Init_Unreadable_Debug(RELVAL *out) {
        RESET_VAL_HEADER(out, REB_BAD_WORD, CELL_FLAG_FIRST_IS_NODE);
        mutable_BINDING(out) = nullptr;

        // While SYM_UNREADABLE might be nice here, this prevents usage at
        // boot time (e.g. data stack initialization)...and it's a good way
        // to crash sites that expect normal voids.  It's usually clear
        // from the assert that the void is unreadable, anyway.
        //
        INIT_VAL_NODE1(out, nullptr);  // FIRST_IS_NODE needed to do this
        return cast(REBVAL*, out);
    }

    #define Init_Unreadable(out) \
        Init_Unreadable_Debug(TRACK_CELL_IF_DEBUG(out))

    #define IS_BAD_WORD_RAW(v) \
        (KIND3Q_BYTE_UNCHECKED(v) == REB_BAD_WORD)

    inline static bool IS_UNREADABLE_DEBUG(const RELVAL *v) {
        if (KIND3Q_BYTE_UNCHECKED(v) != REB_BAD_WORD)
            return false;
        return VAL_NODE1(v) == nullptr;
    }

    #define ASSERT_UNREADABLE_IF_DEBUG(v) \
        assert(IS_UNREADABLE_DEBUG(v))

    #define ASSERT_READABLE_IF_DEBUG(v) \
        assert(not IS_UNREADABLE_DEBUG(v))
#endif


// There are isotope versions and non isotope versions. Usually when messing
// with unsets in the system, the intent is to work with the "stable" form
// (which triggers errors on access).

#define UNSET_VALUE \
    c_cast(const REBVAL*, &PG_Unset_Value)

#define Init_Unset(out) \
    Init_Bad_Word((out), SYM_UNSET)

inline static bool Is_Unset(const RELVAL *v)
  { return Is_Bad_Word_With_Sym(v, SYM_UNSET) and NOT_CELL_FLAG(v, ISOTOPE); }


// Moving a cell invalidates the old location.  This idea is a potential
// prelude to being able to do some sort of reference counting on series based
// on the cells that refer to them tracking when they are overwritten.  In
// the meantime, setting to unreadable void helps see when a value that isn't
// thought to be used any more is still being used.
//
inline static REBVAL *Move_Cell_Untracked(
    RELVAL *out,
    REBVAL *v,
    REBFLGS copy_mask
){
    Copy_Cell_Core(out, v, copy_mask);
  #if defined(NDEBUG)
    Init_Unreadable(v);  // no advantage in release build (yet!)
  #endif
    return cast(REBVAL*, out);
}

#define Move_Cell(out,v) \
    Move_Cell_Untracked(TRACK_CELL_IF_DEBUG(out), (v), CELL_MASK_COPY)

#define Move_Cell_Core(out,v,cell_mask) \
    Move_Cell_Untracked(TRACK_CELL_IF_DEBUG(out), (v), (cell_mask))
