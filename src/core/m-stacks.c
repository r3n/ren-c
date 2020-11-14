//
//  File: %m-stack.c
//  Summary: "data and function call stack implementation"
//  Section: memory
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
// See notes on the stacks in %sys-stack.h
//

#include "sys-core.h"


//
//  Startup_Data_Stack: C
//
void Startup_Data_Stack(REBLEN capacity)
{
    // Start the data stack out with just one element in it, and make it an
    // unreadable void in the debug build.  This helps avoid accidental
    // reads and is easy to notice when it is overwritten.  It also means
    // that indices into the data stack can be unsigned (no need for -1 to
    // mean empty, because 0 can)
    //
    DS_Array = Make_Array_Core(1, ARRAY_FLAG_NULLEDS_LEGAL);
    Init_Unreadable_Void(ARR_HEAD(DS_Array));
    SET_CELL_FLAG(ARR_HEAD(DS_Array), PROTECTED);

    // The END marker will signal DS_PUSH() that it has run out of space,
    // and it will perform the allocation at that time.
    //
    TERM_ARRAY_LEN(DS_Array, 1);
    ASSERT_ARRAY(DS_Array);

    // Reuse the expansion logic that happens on a DS_PUSH() to get the
    // initial stack size.  It requires you to be on an END to run.
    //
    DS_Index = 1;
    DS_Movable_Top = SPECIFIC(ARR_AT(DS_Array, DS_Index));  // can't push RELVALs
    Expand_Data_Stack_May_Fail(capacity);

    DS_DROP();  // drop the hypothetical thing that triggered the expand
}


//
//  Shutdown_Data_Stack: C
//
void Shutdown_Data_Stack(void)
{
    assert(DSP == 0);
    ASSERT_UNREADABLE_IF_DEBUG(ARR_HEAD(DS_Array));

    Free_Unmanaged_Array(DS_Array);
}


//
//  Startup_Frame_Stack: C
//
// We always push one unused frame at the top of the stack.  This way, it is
// not necessary for unused frames to check if `f->prior` is null; it may be
// assumed that it never is.
//
// Also: since frames are needed to track API handles, this permits making
// API handles for things that come into existence at boot and aren't freed
// until shutdown, as they attach to this frame.
//
void Startup_Frame_Stack(void)
{
  #if !defined(NDEBUG) // see Startup_Trash_Debug() for explanation
    assert(IS_POINTER_TRASH_DEBUG(TG_Top_Frame));
    assert(IS_POINTER_TRASH_DEBUG(TG_Bottom_Frame));
    TG_Top_Frame = TG_Bottom_Frame = nullptr;
  #endif

    TG_Frame_Feed_End.index = 0;
    TG_Frame_Feed_End.vaptr = nullptr;
    TG_Frame_Feed_End.array = EMPTY_ARRAY; // for HOLD flag in Push_Frame
    TG_Frame_Feed_End.value = END_NODE;
    TG_Frame_Feed_End.specifier = SPECIFIED;
    TRASH_POINTER_IF_DEBUG(TG_Frame_Feed_End.pending);

    DECLARE_FRAME (f, &TG_Frame_Feed_End, EVAL_MASK_DEFAULT);

    Push_Frame(nullptr, f);

  #ifdef DEBUG_ENSURE_FRAME_EVALUATES
    f->was_eval_called = true;  // fake frame, lie and say it evaluated
  #endif

    TRASH_POINTER_IF_DEBUG(f->prior); // help catch enumeration past FS_BOTTOM
    TG_Bottom_Frame = f;

    assert(FS_TOP == f and FS_BOTTOM == f);
}


//
//  Shutdown_Frame_Stack: C
//
void Shutdown_Frame_Stack(void)
{
    assert(FS_TOP == FS_BOTTOM);

    // To stop enumerations from using nullptr to stop the walk, and not count
    // the bottom frame as a "real stack level", it had a trash pointer put
    // in the debug build.  Restore it to a typical null before the drop.
    //
    assert(IS_POINTER_TRASH_DEBUG(TG_Bottom_Frame->prior));
    TG_Bottom_Frame->prior = nullptr;

  blockscope {
    REBFRM *f = FS_TOP;

    // There's a Catch-22 on checking the balanced state for outstanding
    // manual series allocations, e.g. it can't check *before* the mold buffer
    // is freed because it would look like it was a leaked series, but it
    // can't check *after* because the mold buffer balance check would crash.
    //
    Drop_Frame_Core(f); // can't be Drop_Frame() or Drop_Frame_Unbalanced()

    assert(not FS_TOP);
  }

    TG_Top_Frame = nullptr;
    TG_Bottom_Frame = nullptr;

  #if !defined(NDEBUG)
  blockscope {
    REBSEG *seg = Mem_Pools[FRM_POOL].segs;
    for (; seg != nullptr; seg = seg->next) {
        REBLEN n = Mem_Pools[FRM_POOL].units;
        REBYTE *bp = cast(REBYTE*, seg + 1);
        for (; n > 0; --n, bp += Mem_Pools[FRM_POOL].wide) {
            REBFRM *f = cast(REBFRM*, bp);  // ^-- pool size may be rounded up
            if (IS_FREE_NODE(f))
                continue;
          #ifdef DEBUG_COUNT_TICKS
            printf(
                "** FRAME LEAKED at tick %lu\n",
                cast(unsigned long, f->tick)
            );
          #else
            assert(!"** FRAME LEAKED but DEBUG_COUNT_TICKS not enabled");
          #endif
        }
    }
  }
  #endif
}


//
//  Get_Context_From_Stack: C
//
// Generally speaking, Rebol does not have a "current context" in effect; as
// should you call an `IF` in a function body, there is now a Rebol IF on the
// stack.  But the story for ACTION!s that are implemented in C is different,
// as they have one Rebol action in effect while their C code is in control.
//
// This is used to an advantage in the APIs like rebValue(), to be able to get
// a notion of a "current context" applicable *only* to when natives run.
//
REBCTX *Get_Context_From_Stack(void)
{
    REBFRM *f = FS_TOP;
    REBACT *phase = nullptr; // avoid potential uninitialized variable warning

    for (; f != FS_BOTTOM; f = f->prior) {
        if (not Is_Action_Frame(f))
            continue;

        phase = FRM_PHASE(f);
        break;
    }

    if (f == FS_BOTTOM) {
        //
        // No natives are in effect, so this is API code running directly from
        // an `int main()`.  This is dangerous, as it means any failures will
        // (no TRAP is in effect yet).  For the moment, say such code binds
        // into the user context.
        //
        return VAL_CONTEXT(Get_System(SYS_CONTEXTS, CTX_USER));
    }

    // This would happen if you call the API from something like a traced
    // eval hook, or a Returner_Dispatcher().  For now, just assume that means
    // you want the code to bind into the lib context.
    //
    if (NOT_ACTION_FLAG(phase, IS_NATIVE))
        return VAL_CONTEXT(Lib_Context);

    REBARR *details = ACT_DETAILS(phase);
    REBVAL *context = SPECIFIC(ARR_AT(details, 1));
    return VAL_CONTEXT(context);
}


//
//  Expand_Data_Stack_May_Fail: C
//
// The data stack maintains an invariant that you may never push an END to it.
// So each push looks to see if it's pushing to a cell that contains an END
// and if so requests an expansion.
//
// WARNING: This will invalidate any extant pointers to REBVALs living in
// the stack.  It is for this reason that stack access should be done by
// REBDSP "data stack pointers" and not by REBVAL* across *any* operation
// which could do a push or pop.  (Currently stable w.r.t. pop but there may
// be compaction at some point.)
//
REBVAL *Expand_Data_Stack_May_Fail(REBLEN amount)
{
    REBLEN len_old = ARR_LEN(DS_Array);

    // The current requests for expansion should only happen when the stack
    // is at its end.  Sanity check that.
    //
    assert(len_old == DS_Index);
    assert(IS_END(DS_Movable_Top));
    assert(DS_Movable_Top == SPECIFIC(ARR_TAIL(DS_Array)));
    assert(DS_Movable_Top - SPECIFIC(ARR_HEAD(DS_Array)) == cast(int, len_old));

    // If adding in the requested amount would overflow the stack limit, then
    // give a data stack overflow error.
    //
    if (SER_REST(SER(DS_Array)) + amount >= STACK_LIMIT) {
        //
        // Because the stack pointer was incremented and hit the END marker
        // before the expansion, we have to decrement it if failing.
        //
        --DS_Index;
        Fail_Stack_Overflow(); // !!! Should this be a "data stack" message?
    }

    Extend_Series(SER(DS_Array), amount);

    // Update the pointer used for fast access to the top of the stack that
    // likely was moved by the above allocation (needed before using DS_TOP)
    //
    DS_Movable_Top = cast(REBVAL*, ARR_AT(DS_Array, DS_Index));

    // We fill in the data stack with "GC safe trash" (which is void in the
    // release build, but will raise an alarm if VAL_TYPE() called on it in
    // the debug build).  In order to serve as a marker for the stack slot
    // being available, it merely must not be IS_END()...

    REBVAL *cell = DS_Movable_Top;

    REBLEN len_new = len_old + amount;
    REBLEN n;
    for (n = len_old; n < len_new; ++n, ++cell)
        Init_Unreadable_Void(cell);

    // Update the end marker to serve as the indicator for when the next
    // stack push would need to expand.
    //
    TERM_ARRAY_LEN(DS_Array, len_new);
    assert(cell == ARR_TAIL(DS_Array));

    ASSERT_ARRAY(DS_Array);
    return DS_TOP;
}


//
//  Pop_Stack_Values_Core: C
//
// Pops computed values from the stack to make a new ARRAY.
//
REBARR *Pop_Stack_Values_Core(REBDSP dsp_start, REBFLGS flags)
{
    REBARR *array = Copy_Values_Len_Shallow_Core(
        DS_AT(dsp_start + 1), // start somewhere in the stack, end at DS_TOP
        SPECIFIED, // data stack should be fully specified--no relative values
        DSP - dsp_start, // len
        flags
    );

    DS_DROP_TO(dsp_start);
    return array;
}


//
//  Pop_Stack_Values_Into: C
//
// Pops computed values from the stack into an existing ANY-ARRAY.  The
// index of that array will be updated to the insertion tail (/INTO protocol)
//
void Pop_Stack_Values_Into(REBVAL *into, REBDSP dsp_start) {
    REBLEN len = DSP - dsp_start;
    REBVAL *values = SPECIFIC(ARR_AT(DS_Array, dsp_start + 1));

    VAL_INDEX_RAW(into) = Insert_Series(
        VAL_SERIES_ENSURE_MUTABLE(into),
        VAL_INDEX(into),
        cast(REBYTE*, values), // stack only holds fully specified REBVALs
        len // multiplied by width (sizeof(REBVAL)) in Insert_Series
    );

    DS_DROP_TO(dsp_start);
}
