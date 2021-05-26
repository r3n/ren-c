//
//  File: %sys-trash.h
//  Summary: "Unreadable Variant of BAD-WORD! Available In Early Boot"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
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
// The debug build has the concept of making an unreadable "trash" cell that
// will fail on most forms of access in the system.  However, it will behave
// neutrally as far as the garbage collector is concerned.  This means that
// it can be used as a placeholder for a value that will be filled in at
// some later time--spanning an evaluation.
//
// Although the low-level type used to store these cells is REB_BAD_WORD, it
// will panic if you try to test it with IS_BAD_WORD(), and will also refuse
// VAL_TYPE() checks.  The only way to check if something IS_TRASH() is in the
// debug build, and hence should only appear in asserts.
//
// This is useful anytime a placeholder is needed in a slot temporarily where
// the code knows it's supposed to come back and fill in the correct thing
// later.  The panics help make sure it is never actually read.
//

#if !defined(DEBUG_UNREADABLE_TRASH)  // release behavior, just ~trash~
    inline static REBVAL *Init_Bad_Word_Core(
        RELVAL *out,
        const REBSYM *label,
        REBFLGS flags
    );  // forward decl

    #define Init_Trash(v) \
        Init_Bad_Word_Core((v), PG_Trash_Canon, CELL_MASK_NONE)
#else
    inline static REBVAL *Init_Trash_Untracked(RELVAL *out) {
        RESET_VAL_HEADER(out, REB_BAD_WORD, CELL_FLAG_FIRST_IS_NODE);
        mutable_BINDING(out) = nullptr;

        // While SYM_UNREADABLE might be nice here, this prevents usage at
        // boot time (e.g. data stack initialization)...and it's a good way
        // to crash sites that expect normal voids.  It's usually clear
        // from the assert that it's unreadable, anyway.
        //
        INIT_VAL_NODE1(out, nullptr);  // FIRST_IS_NODE needed to do this
        return cast(REBVAL*, out);
    }

    #define Init_Trash(out) \
        Init_Trash_Untracked(TRACK_CELL_IF_DEBUG(out))

    inline static bool IS_TRASH(const RELVAL *v) {
        if (KIND3Q_BYTE_UNCHECKED(v) != REB_BAD_WORD)
            return false;
        return VAL_NODE1(v) == nullptr;
    }
#endif
