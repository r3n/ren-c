//
//  File: %sys-void.h
//  Summary: "VOID! Datatype Header"
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
// Void! results are the default for `do []`, and unlike NULL a void! *is*
// a value...however a somewhat unfriendly one.  While NULLs are falsey, void!
// is *neither* truthy nor falsey.  Though a void! can be put in an array (a
// NULL can't) if the evaluator tries to run a void! cell in an array, it will
// trigger an error.
//
// Void! also comes into play in what is known as "voidification" of NULLs.
// Loops wish to reserve NULL as the return result if there is a BREAK, and
// conditionals like IF and SWITCH want to reserve NULL to mean there was no
// branch taken.  So when branches or loop bodies produce null, they need
// to be converted to some ANY-VALUE!.
//
// The console doesn't print anything for void! evaluation results by default,
// so that routines like HELP won't have additional output than what they
// print out.
//
// In the debug build, it is possible to make an "unreadable" void!.  This
// will behave neutrally as far as the garbage collector is concerned, so
// it can be used as a placeholder for a value that will be filled in at
// some later time--spanning an evaluation.  But if the special IS_UNREADABLE
// checks are not used, it will not respond to IS_VOID() and will also
// refuse VAL_TYPE() checks.  This is useful anytime a placeholder is needed
// in a slot temporarily where the code knows it's supposed to come back and
// fill in the correct thing later...where the asserts serve as a reminder
// if that fill in never happens.
//

inline static REBVAL *Init_Labeled_Void(RELVAL *out, const REBSTR *label) {
    RESET_CELL(out, REB_VOID, CELL_FLAG_FIRST_IS_NODE);
    VAL_NODE(out) = NOD(label);
    return cast(REBVAL*, out);
}

#define Init_Void(out,sym) \
    Init_Labeled_Void((out), Canon(sym))

inline static REBVAL *Init_Unlabeled_Void(RELVAL *out) {
    RESET_CELL(out, REB_VOID, CELL_FLAG_FIRST_IS_NODE);
    VAL_NODE(out) = nullptr;
    return cast(REBVAL*, out);
}

inline static const REBSTR *VAL_VOID_OPT_LABEL(REBCEL(const*) v) {
    assert(CELL_KIND(v) == REB_VOID);
    assert(GET_CELL_FLAG(v, FIRST_IS_NODE));
    return cast(const REBSTR*, VAL_NODE(v));
}

// Don't let SYM_0 be used for unlabeled void, in case checking for a match
// with a symbol extracted from a WORD! which has no symbol shorthand.
//
inline static bool Is_Void_With_Sym(const RELVAL *v, REBSYM sym) {
    assert(sym != SYM_0);
    if (not IS_VOID(v))
        return false;
    const REBSTR *label = VAL_VOID_OPT_LABEL(v);
    if (not label)
        return false;  // unlabeled
    return cast(REBLEN, sym) == cast(REBLEN, STR_SYMBOL(label));
}


// Many loop constructs use BLANK! as a unique signal that the loop body
// never ran, e.g. `for-each x [] [<unreturned>]` or `loop 0 [<unreturned>]`.
// It's more valuable to have that signal be unique and have it be falsey
// than it is to be able to return BLANK! from a loop, so blanks are voidified
// alongside NULL (reserved for BREAKing)
//
inline static REBVAL *Voidify_If_Nulled_Or_Blank(REBVAL *cell) {
    if (IS_NULLED(cell))
        Init_Void(cell, SYM_NULLED);
    else if (IS_BLANK(cell))
        Init_Void(cell, SYM_BLANKED);
    return cell;
}


#if !defined(DEBUG_UNREADABLE_VOIDS)  // release behavior, same as plain VOID!
    #define Init_Unreadable_Void(v) \
        Init_Unlabeled_Void(v)

    #define IS_VOID_RAW(v) \
        IS_BLANK(v)

    #define ASSERT_UNREADABLE_IF_DEBUG(v) \
        assert(IS_VOID(v))  // would have to be a void even if not unreadable

    #define ASSERT_READABLE_IF_DEBUG(v) \
        NOOP
#else
    inline static REBVAL *Init_Unreadable_Void_Debug(
        RELVAL *out, const char *file, int line
    ){
        RESET_CELL_Debug(out, REB_VOID, CELL_FLAG_FIRST_IS_NODE, file, line);

        // While SYM_UNREADABLE would be nice here, that prevents usage at
        // boot time (e.g. data stack initialization).  It's usually clear
        // from the assert that the void is unreadable, anyway.
        //
        VAL_NODE(out) = nullptr;  // needs flag for VAL_NODE() to not assert
        out->extra.tick = -1;  // even non-tick counting builds default to 1
        return cast(REBVAL*, out);
    }

    #define Init_Unreadable_Void(out) \
        Init_Unreadable_Void_Debug((out), __FILE__, __LINE__)

    #define IS_VOID_RAW(v) \
        (KIND3Q_BYTE_UNCHECKED(v) == REB_VOID)

    inline static bool IS_UNREADABLE_DEBUG(const RELVAL *v) {
        if (KIND3Q_BYTE_UNCHECKED(v) != REB_VOID)
            return false;
        return v->extra.tick < 0;
    }

    #define ASSERT_UNREADABLE_IF_DEBUG(v) \
        assert(IS_UNREADABLE_DEBUG(v))

    #define ASSERT_READABLE_IF_DEBUG(v) \
        assert(not IS_UNREADABLE_DEBUG(v))
#endif
