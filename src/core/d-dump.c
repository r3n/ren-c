//
//  File: %d-dump.c
//  Summary: "various debug output functions"
//  Section: debug
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
// Most of these low-level debug routines were leftovers from R3-Alpha, which
// had no DEBUG build (and was perhaps frequently debugged without an IDE
// debugger).  After the open source release, Ren-C's reliance is on a
// more heavily checked debug build...so these routines were not used.
//
// They're being brought up to date to be included in the debug build only
// version of panic().  That should keep them in working shape.
//
// Note: These routines use `printf()`, which is only linked in DEBUG builds.
// Higher-level Rebol formatting should ultimately be using BLOCK! dialects,
// as opposed to strings with %s and %d.  Bear in mind the "z" modifier in
// printf is unavailable in C89, so if something might be 32-bit or 64-bit
// depending, it must be cast to unsigned long:
//
// http://stackoverflow.com/q/2125845
//

#include "sys-core.h"

#if !defined(NDEBUG)

#ifdef _MSC_VER
#define snprintf _snprintf
#endif


//
//  Dump_Series: C
//
void Dump_Series(REBSER *s, const char *memo)
{
    printf("Dump_Series(%s) @ %p\n", memo, cast(void*, s));
    fflush(stdout);

    if (s == NULL)
        return;

    printf(" wide: %d\n", cast(int, SER_WIDE(s)));
    printf(" size: %ld\n", cast(unsigned long, SER_TOTAL_IF_DYNAMIC(s)));
    if (IS_SER_DYNAMIC(s))
        printf(" bias: %d\n", cast(int, SER_BIAS(s)));
    printf(" used: %d\n", cast(int, SER_USED(s)));
    printf(" rest: %d\n", cast(int, SER_REST(s)));

    // flags includes len if non-dynamic
    printf(" flags: %lx\n", cast(unsigned long, s->leader.bits));

    // info includes width
    printf(" info: %lx\n", cast(unsigned long, SER_INFO(s)));

    fflush(stdout);
}


//
//  Dump_Info: C
//
void Dump_Info(void)
{
    printf("^/--REBOL Kernel Dump--\n");

    printf("Evaluator:\n");
    printf("    Cycles:  %ld\n", cast(unsigned long, Eval_Cycles));
    printf("    Counter: %d\n", cast(int, Eval_Count));
    printf("    Dose:    %d\n", cast(int, Eval_Dose));
    printf("    Signals: %lx\n", cast(unsigned long, Eval_Signals));
    printf("    Sigmask: %lx\n", cast(unsigned long, Eval_Sigmask));
    printf("    DSP:     %ld\n", cast(unsigned long, DSP));

    printf("Memory/GC:\n");

    printf("    Ballast: %d\n", cast(int, GC_Ballast));
    printf("    Disable: %s\n", GC_Disabled ? "yes" : "no");
    printf("    Guarded Nodes: %d\n", cast(int, SER_USED(GC_Guarded)));
    fflush(stdout);
}


//
//  Dump_Stack: C
//
// Prints stack counting levels from the passed in number.  Pass 0 to start.
//
void Dump_Stack(REBFRM *f, REBLEN level)
{
    printf("\n");

    if (f == FS_BOTTOM) {
        printf("*STACK[] - NO FRAMES*\n");
        fflush(stdout);
        return;
    }

    printf(
        "STACK[%d](%s) - %d\n",
        cast(int, level),
        Frame_Label_Or_Anonymous_UTF8(f),
        KIND3Q_BYTE(f->feed->value)
    );

    if (not Is_Action_Frame(f)) {
        printf("(no function call pending or in progress)\n");
        fflush(stdout);
        return;
    }

    fflush(stdout);

    REBINT n = 1;
    REBVAL *arg = FRM_ARG(f, 1);
    const REBKEY *tail;
    const REBKEY *key = ACT_KEYS(&tail, FRM_PHASE(f));

    for (; key != tail; ++key, ++arg, ++n) {
        if (IS_NULLED(arg))
            printf(
                "    %s:\n",
                STR_UTF8(KEY_SYMBOL(key))
            );
        else
            printf(
                "    %s: %p\n",
                STR_UTF8(KEY_SYMBOL(key)),
                cast(void*, arg)
            );
    }

    if (f->prior != FS_BOTTOM)
        Dump_Stack(f->prior, level + 1);
}



#endif // DUMP is picked up by scan regardless of #ifdef, must be defined


//
//  dump: native [
//
//  "Temporary debug dump"
//
//      return: [<invisible>]
//      :value [word!]
//  ]
//
REBNATIVE(dump)
{
    INCLUDE_PARAMS_OF_DUMP;

#ifdef NDEBUG
    UNUSED(ARG(value));
    fail (Error_Debug_Only_Raw());
#else
    REBVAL *v = ARG(value);

    PROBE(v);
    printf("=> ");
    if (IS_WORD(v)) {
        const REBVAL* var = try_unwrap(Lookup_Word(v, SPECIFIED));
        if (not var) {
            PROBE("\\unbound\\");
        }
        else if (IS_NULLED(var)) {
            PROBE("\\null\\");
        }
        else
            PROBE(var);
    }

    RETURN_INVISIBLE;
#endif
}
