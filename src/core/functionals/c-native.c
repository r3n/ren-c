//
//  File: %c-native.c
//  Summary: "Function that executes implementation as native code"
//  Section: datatypes
//  Project: "Ren-C Language Interpreter and Run-time Environment"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2020 Ren-C Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the GNU Lesser General Public License (LGPL), Version 3.0.
// You may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.gnu.org/licenses/lgpl-3.0.en.html
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A native is unique from other function types, because instead of there
// being a "Native_Dispatcher()", each native has a C function that acts as
// its dispatcher.
//
// Also unique about natives is that the native function constructor must be
// built "by hand", since it is required to get the ball rolling on having
// functions to call at all.
//
// If there *were* a REBNATIVE(native) this would be its spec:
//
//  native: native [
//      spec [block!]
//      /body "Body of equivalent usermode code (for documentation)}
//          [block!]
//  ]
//

#include "sys-core.h"


//
//  Make_Native: C
//
// Reused function in Startup_Natives() as well as extensions loading natives,
// which can be parameterized with a different context in which to look up
// bindings by deafault in the API when that native is on the stack.
//
// Each entry should be one of these forms:
//
//    some-name: native [spec content]
//
//    some-name: native/body [spec content] [equivalent user code]
//
// It is optional to put ENFIX between the SET-WORD! and the spec.
//
// If more refinements are added, this will have to get more sophisticated.
//
// Though the manual building of this table is not as "nice" as running the
// evaluator, the evaluator makes comparisons against native values.  Having
// all natives loaded fully before ever running Eval_Core() helps with
// stability and invariants...also there's "state" in keeping track of which
// native index is being loaded, which is non-obvious.  But these issues
// could be addressed (e.g. by passing the native index number / DLL in).
//
REBVAL *Make_Native(
    RELVAL **item, // the item will be advanced as necessary
    const RELVAL *tail,
    REBSPC *specifier,
    REBNAT dispatcher,
    const REBVAL *module
){
    assert(specifier == SPECIFIED); // currently a requirement

    // Get the name the native will be started at with in Lib_Context
    //
    if (not IS_SET_WORD(*item))
        panic (*item);

    REBVAL *name = SPECIFIC(*item);
    ++*item;

    bool enfix;
    if (IS_WORD(*item) and VAL_WORD_ID(*item) == SYM_ENFIX) {
        enfix = true;
        ++*item;
    }
    else
        enfix = false;

    // See if it's being invoked with NATIVE or NATIVE/BODY
    //
    bool has_body;
    if (IS_WORD(*item)) {
        if (VAL_WORD_ID(*item) != SYM_NATIVE)
            panic (*item);
        has_body = false;
    }
    else {
        DECLARE_LOCAL (temp);
        if (
            VAL_WORD_ID(VAL_SEQUENCE_AT(temp, *item, 0)) != SYM_NATIVE
            or VAL_WORD_ID(VAL_SEQUENCE_AT(temp, *item, 1)) != SYM_BODY
        ){
            panic (*item);
        }
        has_body = true;
    }
    ++*item;

    const REBVAL *spec = SPECIFIC(*item);
    ++*item;
    if (spec == tail or not IS_BLOCK(spec))
        panic (spec);

    // With the components extracted, generate the native and add it to
    // the Natives table.  The associated C function is provided by a
    // table built in the bootstrap scripts, `Native_C_Funcs`.

    REBCTX *meta;
    REBFLGS flags = MKF_KEYWORDS | MKF_RETURN;
    REBARR *paramlist = Make_Paramlist_Managed_May_Fail(
        &meta,
        spec,
        &flags  // return type checked only in debug build
    );
    ASSERT_SERIES_TERM_IF_NEEDED(paramlist);

    // Natives are their own dispatchers; there is no point of interjection
    // to force their outputs to anything but what they return.  Instead of
    // `return: <void>` use `return: [void!]` and `return Init_Void(D_OUT);`
    // And instead of `return: <elide>` use `return: [<invisible>]` along
    // with `return D_OUT;`...having made no modifications to D_OUT.
    //
    assert(not (flags & MKF_IS_VOIDER));
    assert(not (flags & MKF_IS_ELIDER));

    REBACT *native = Make_Action(
        paramlist,
        dispatcher, // "dispatcher" is unique to this "native"
        IDX_NATIVE_MAX // details array capacity
    );

    assert(ACT_META(native) == nullptr);
    mutable_ACT_META(native) = meta;

    SET_ACTION_FLAG(native, IS_NATIVE);
    if (enfix)
        SET_ACTION_FLAG(native, ENFIXED);

    REBARR *details = ACT_DETAILS(native);

    // If a user-equivalent body was provided, we save it in the native's
    // REBVAL for later lookup.
    //
    if (has_body) {
        if (not IS_BLOCK(*item))
            panic (*item);

        Derelativize(ARR_AT(details, IDX_NATIVE_BODY), *item, specifier);
        ++*item;
    }
    else
        Init_Blank(ARR_AT(details, IDX_NATIVE_BODY));

    // When code in the core calls APIs like `rebValue()`, it consults the
    // stack and looks to see where the native function that is running
    // says its "module" is.  Core natives default to Lib_Context.
    //
    Copy_Cell(ARR_AT(details, IDX_NATIVE_CONTEXT), module);

    // Append the native to the module under the name given.
    //
    REBVAL *var = Append_Context(VAL_CONTEXT(module), name, nullptr);
    Init_Action(var, native, VAL_WORD_SYMBOL(name), UNBOUND);

    return var;
}


//
//  Init_Action_Meta_Shim: C
//
// Make_Paramlist_Managed_May_Fail() needs the object archetype ACTION-META
// from %sysobj.r, to have the keylist to use in generating the info used
// by HELP for the natives.  However, natives themselves are used in order
// to run the object construction in %sysobj.r
//
// To break this Catch-22, this code builds a field-compatible version of
// ACTION-META.  After %sysobj.r is loaded, an assert checks to make sure
// that this manual construction actually matches the definition in the file.
//
static void Init_Action_Meta_Shim(void) {
    SYMID field_syms[3] = {
        SYM_DESCRIPTION, SYM_PARAMETER_TYPES, SYM_PARAMETER_NOTES
    };
    REBCTX *meta = Alloc_Context_Core(REB_OBJECT, 4, NODE_FLAG_MANAGED);
    REBLEN i = 1;
    for (; i != 4; ++i)
        Init_Nulled(Append_Context(meta, nullptr, Canon(field_syms[i - 1])));

    Root_Action_Meta = Init_Object(Alloc_Value(), meta);
    Force_Value_Frozen_Deep(Root_Action_Meta);
}

static void Shutdown_Action_Meta_Shim(void) {
    rebRelease(Root_Action_Meta);
}


//
//  Startup_Natives: C
//
// Create native functions.  In R3-Alpha this would go as far as actually
// creating a NATIVE native by hand, and then run code that would call that
// native for each function.  Ren-C depends on having the native table
// initialized to run the evaluator (for instance to test functions against
// the UNWIND native's FUNC signature in definitional returns).  So it
// "fakes it" just by calling a C function for each item...and there is no
// actual "native native".
//
// Returns an array of words bound to natives for SYSTEM/CATALOG/NATIVES
//
REBARR *Startup_Natives(const REBVAL *boot_natives)
{
    // Must be called before first use of Make_Paramlist_Managed_May_Fail()
    //
    Init_Action_Meta_Shim();

    assert(VAL_INDEX(boot_natives) == 0); // should be at head, sanity check
    const RELVAL *tail;
    RELVAL *item = VAL_ARRAY_KNOWN_MUTABLE_AT(&tail, boot_natives);
    REBSPC *specifier = VAL_SPECIFIER(boot_natives);

    // Although the natives are not being "executed", there are typesets
    // being built from the specs.  So to process `foo: native [x [integer!]]`
    // the INTEGER! word must be bound to its datatype.  Deep walk the
    // natives in order to bind these datatypes.
    //
    Bind_Values_Deep(item, tail, Lib_Context);

    REBARR *catalog = Make_Array(Num_Natives);

    REBLEN n = 0;
    REBVAL *generic_word = nullptr; // gives clear error if GENERIC not found

    while (item != tail) {
        if (n >= Num_Natives)
            panic (item);

        REBVAL *name = SPECIFIC(item);
        assert(IS_SET_WORD(name));

        REBVAL *native = Make_Native(
            &item,
            tail,
            specifier,
            Native_C_Funcs[n],
            Lib_Context
        );

        // While the lib context natives can be overwritten, the system
        // currently depends on having a permanent list of the natives that
        // does not change, see uses via NATIVE_VAL() and NAT_ACT().
        //
        Natives[n] = VAL_ACTION(native);  // Note: Loses enfixedness (!)

        REBVAL *catalog_item = Copy_Cell(Alloc_Tail_Array(catalog), name);
        mutable_KIND3Q_BYTE(catalog_item) = REB_WORD;
        mutable_HEART_BYTE(catalog_item) = REB_WORD;

        if (VAL_WORD_ID(name) == SYM_GENERIC)
            generic_word = name;

        ++n;
    }

    if (n != Num_Natives)
        panic ("Incorrect number of natives found during processing");

    if (not generic_word)
        panic ("GENERIC native not found during boot block processing");

    return catalog;
}


//
//  Shutdown_Natives: C
//
// Being able to run Recycle() during the native startup process means being
// able to holistically check the system state.  This relies on initialized
// data in the natives table.  Since the interpreter can be shutdown and
// started back up in the same session, we can't rely on zero initialization
// for startups after the first, unless we manually null them out.
//
void Shutdown_Natives(void) {
    REBLEN n;
    for (n = 0; n < Num_Natives; ++n)
        Natives[n] = nullptr;

    Shutdown_Action_Meta_Shim();
}
