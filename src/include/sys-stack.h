//
//  File: %sys-stack.h
//  Summary: {Definitions for "Data Stack" and the C stack}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2018 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The data stack (DS_) is for pushing one individual REBVAL at a time.  The
// values can then be popped in a Last-In-First-Out way.  It is also possible
// to mark a stack position, do any number of pushes, and then ask for the
// range of values pushed since the mark to be placed into a REBARR array.
// As long as a value is on the data stack, any series it refers to will be
// protected from being garbage-collected.
//
// A notable usage of the data stack is by REDUCE and COMPOSE.  They use it
// to gather values prior to their insertion into a final array.  It's better
// for many clients to use the data stack as a common preallocated working
// space.  This way the size of the accumulated result is known, preventing
// wasting space on expansions or resizes and shuffling due to a guessed size.
//
// The data stack has many applications, and can be used by any piece of the
// system.  But there is a rule that when that piece is finished, it must
// "balance" the stack back to where it was when it was called!  There is
// a check in the main evaluator loop that the stack has been balanced to
// wherever it started by the time a function call ends.  However, it is not
// necessary to balance the stack in the case of calling a `fail`--because
// it will be automatically restored to where it was at the PUSH_TRAP().
//
// To speed pushes and pops to the stack while also making sure that each
// push is tested to see if an expansion is needed, a trick is used.  This
// trick is to grow the stack in blocks, and always maintain that the block
// has an END marker at its point of capacity--and ensure that there are no
// end markers between the DSP and that capacity.  This way, if a push runs
// up against an END it knows to do an expansion.
//
//=// NOTES ///////////////////////////////////////////////////////////////=//
//
// * The data stack is limited in size, and this means code that uses it may
//   break down when working on larger cases:
//
//   https://github.com/metaeducation/ren-c/issues/679
//
// * Although R3-Alpha used the data stack for pushing function arguments,
//   the arguments were frequently passed around by pointer (vs. using an
//   indexed "DSP" position).  This was bad since the data stack could
//   relocate its contents due to growth.  For this and oher reasons,
//   the Rebol call stack is built out of linked REBARR allocations which
//   can be used to back FRAME! contexts.
//


// DSP stands for "(D)ata (S)tack "(P)osition", and is the index of the top
// of the data stack (last valid item in the underlying array)
//
#define DSP \
    cast(REBDSP, DS_Index) // cast helps stop ++DSP, etc.

// DS_TOP is the most recently pushed item.
//
#define DS_TOP \
    cast(REBVAL*, DS_Movable_Top) // cast helps stop ++DS_TOP, etc.

// DS_AT accesses value at given stack location.  It is allowed to point at
// a stack location that is an end, e.g. DS_AT(dsp + 1), because that location
// may be used as the start of a copy which is ultimately of length 0.
//
inline static REBVAL *DS_AT(REBDSP d) {
    REBVAL *at = SPECIFIC(ARR_HEAD(DS_Array) + d);
    assert(
        ((at->header.bits & NODE_FLAG_CELL) and d <= (DSP + 1))
        or (not (SECOND_BYTE(at->header) != REB_0 and d == (DSP + 1)))
    );
    return at;
}

#if !defined(NDEBUG)
    #define IN_DATA_STACK_DEBUG(v) \
        IS_VALUE_IN_ARRAY_DEBUG(DS_Array, (v))
#endif

//
// PUSHING
//
// If you push "unsafe" trash to the stack, it has the benefit of costing
// nothing extra in a release build for setting the value (as it is just
// left uninitialized).  But you must make sure that a GC can't run before
// you have put a valid value into the slot you pushed.
//
// If the stack runs out of capacity then it will be expanded by the basis
// defined below.  The number is arbitrary and should be tuned.  Note the
// number of bytes will be sizeof(REBVAL) * STACK_EXPAND_BASIS
//

#define STACK_EXPAND_BASIS 128

// Note: DS_Movable_Top is DS_TOP, but it asserts on ENDs...
//
#define DS_PUSH() \
    (++DS_Index, ++DS_Movable_Top, IS_END(DS_Movable_Top) \
        ? Expand_Data_Stack_May_Fail(STACK_EXPAND_BASIS) \
        : TRASH_CELL_IF_DEBUG(DS_Movable_Top)) \


//
// POPPING
//
// Since it's known that END markers were never pushed, a pop can just leave
// whatever bits had been previously pushed, dropping only the index.  The
// only END marker will be the one indicating the tail of the stack.  
//

#ifdef NDEBUG
    #define DS_DROP() \
        (--DS_Index, --DS_Movable_Top)

    #define DS_DROP_TO(dsp) \
        (DS_Movable_Top -= (DS_Index - (dsp)), DS_Index = (dsp))
#else
    inline static void DS_DROP(void) {
        Init_Unreadable_Void(DS_TOP); // mostly trashy but safe for NOT_END()
        --DS_Index;
        --DS_Movable_Top;
    }

    inline static void DS_DROP_TO(REBDSP dsp) {
        assert(DSP >= dsp);
        while (DSP != dsp)
            DS_DROP();
    }
#endif

// If Pop_Stack_Values_Core is used ARRAY_HAS_FILE_LINE, it means the system
// will try to capture the file and line number associated with the current
// frame into the generated array.  But if there are other flags--like
// ARRAY_FLAG_IS_PARAMLIST or ARRAY_FLAG_IS_VARLIST--you don't want to do
// this, because the ->link and ->misc fields have other uses.
//
#define Pop_Stack_Values(dsp) \
    Pop_Stack_Values_Core((dsp), ARRAY_MASK_HAS_FILE_LINE)


//=////////////////////////////////////////////////////////////////////////=//
//
//  C STACK
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Rebol doesn't want to crash in the event of a stack overflow, but would
// like to gracefully trap it and return the user to the console.  While it
// is possible for Rebol to set a limit to how deeply it allows function
// calls in the interpreter to recurse, there's no *portable* way to
// catch a stack overflow in the C code of the interpreter itself.
//
// Hence, by default Rebol will use a non-standard heuristic.  A flag is
// passed to say if OS_STACK_GROWS_UP.  If so, it then extrapolates that C
// function call frames will be laid out consecutively, and the memory
// difference between a stack variable in the topmost stacks can be checked
// against some limit.
//
// This has nothing to do with guarantees in the C standard, and compilers
// can really put variables at any address they feel like:
//
// http://stackoverflow.com/a/1677482/211160
//
// Additionally, it puts the burden on every recursive or deeply nested
// routine to sprinkle calls to the C_STACK_OVERFLOWING macro somewhere
// in it.  The ideal answer is to make Rebol itself corral an interpreted
// script such that it can't cause the C code to stack overflow.  Lacking
// that ideal this technique could break, so build configurations should
// be able to turn it off if needed.
//
// In the meantime, C_STACK_OVERFLOWING is a macro which takes the
// address of some variable local to the currently executed function.
// Note that because the limit is noticed before the C stack has *actually*
// overflowed, you still have a bit of stack room to do the cleanup and
// raise an error trap.  (You need to take care of any unmanaged series
// allocations, etc).  So cleaning up that state should be doable without
// making deep function calls.
//
// !!! Future approaches should look into use of Windows stack exceptions
// or libsigsegv:
//
// http://stackoverflow.com/questions/5013806/
//

#if defined(TO_EMSCRIPTEN)
    //
    // !!! Catching stack overflows in emscripten stopped working in the
    // BinaryEn build; the stack seems to not grow up or down specifically.
    // As a temporary non-solution, see what happens to just let it crash.
    //
    #define C_STACK_OVERFLOWING(address_of_local_var) \
        false

#elif defined(OS_STACK_GROWS_UP)

    #define C_STACK_OVERFLOWING(address_of_local_var) \
        (cast(uintptr_t, (address_of_local_var)) >= TG_Stack_Limit)

#elif defined(OS_STACK_GROWS_DOWN)

    #define C_STACK_OVERFLOWING(address_of_local_var) \
        (cast(uintptr_t, (address_of_local_var)) <= TG_Stack_Limit)

#else

    #define C_STACK_OVERFLOWING(address_of_local_var) \
        (TG_Stack_Grows_Up \
            ? cast(uintptr_t, (address_of_local_var)) >= TG_Stack_Limit \
            : cast(uintptr_t, (address_of_local_var)) <= TG_Stack_Limit)
#endif


// !!! This could be made configurable.  However, it needs to be initialized
// early in the boot process.  It may be that some small limit is used enough
// for boot, that can be expanded by native calls later.
//
// !!! Had note that said "made somewhat smaller than linker setting to allow
// trapping it".  But there's no corresponding linker setting.
//
#define DEFAULT_STACK_BOUNDS (2*1024*1024)

// Since stack overflows are memory-related errors, don't try to do any
// error allocations...just use an already made error.
//
#define Fail_Stack_Overflow() \
    fail (VAL_CONTEXT(Root_Stackoverflow_Error));
