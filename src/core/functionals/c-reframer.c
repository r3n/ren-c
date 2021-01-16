//
//  File: %c-reframer.c
//  Summary: "Function that can transform arbitrary callsite functions"
//  Section: datatypes
//  Project: "Ren-C Language Interpreter and Run-time Environment"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2021 Ren-C Open Source Contributors
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
// REFRAMER allows one to define a function that does generalized transforms
// on the input (and output) of other functions.  Unlike ENCLOSE, it does not
// specify an exact function it does surgery on the frame of ahead of time.
// Instead, each invocation of the reframing action interacts with the
// instance that follows it at the callsite.
//
// A simple example is a function which removes quotes from the first
// parameter to a function, and adds them back for the result:
//
//     requote: reframer func [f [frame!]] [
//         p: first words of f
//         num-quotes: quotes of f/(p)
//
//         f/(p): dequote f/(p)
//
//         return quote/depth do f num-quotes
//     ]
//
//     >> item: first ['''[a b c]]
//
//     >> requote append item <d>  ; append doesn't accept QUOTED! items
//     == '''[a b c <d>]   ; munging frame and result makes it seem to
//
// !!! Due to the way that REFRAMER works today, it cannot support a chain
// of reframers.  e.g. with MY implemented as a reframer, you couldn't say:
//
//     >> item: my requote append <d>
//
// Being able to do so would require some kind of "compound frame" that could
// allow MY to push through REQUOTE to see APPEND's arguments.  This sounds
// technically difficult, though perhaps pared down versions could be made
// in the near term (e.g. in cases like this, where the reframer takes no
// arguments of its own)
//

#include "sys-core.h"

enum {
    IDX_REFRAMER_SHIM = 1,  // action that can manipulate the reframed frame
    IDX_REFRAMER_PARAM_INDEX,  // index in shim that receives FRAME!
    IDX_REFRAMER_MAX
};


//
//  Make_Invocation_Frame_Throws: C
//
// This builds a frame from a feed *as if* it were going to be used to call
// an action, but doesn't actually make the call.  Instead it leaves the
// varlist available for other purposes.
//
bool Make_Invocation_Frame_Throws(REBFRM *f, const REBVAL *action)
{
    assert(IS_ACTION(action));
    assert(f == FS_TOP);

    // It is desired that any nulls encountered be processed as if they are
    // not specialized...and gather at the callsite if necessary.
    //
    f->flags.bits |=
        EVAL_FLAG_ERROR_ON_DEFERRED_ENFIX;  // can't deal with ELSE/THEN/etc.

    option(const REBSYM*) label = nullptr;  // !!! for now
    Push_Action(f, VAL_ACTION(action), VAL_ACTION_BINDING(action));
    Begin_Prefix_Action(f, label);

    // Use this special mode where we ask the dispatcher not to run, just to
    // gather the args.  Push_Action() checks that it's not set, so we don't
    // set it until after that.
    //
    SET_EVAL_FLAG(f, FULFILL_ONLY);

    assert(FRM_BINDING(f) == VAL_ACTION_BINDING(action));  // no invocation

    bool threw = Process_Action_Throws(f);

    assert(NOT_EVAL_FLAG(f, FULFILL_ONLY));  // cleared by the evaluator

    // Drop_Action() clears out the phase and binding.  Put them back.
    // !!! Should it check EVAL_FLAG_FULFILL_ONLY?

    INIT_FRM_PHASE(f, VAL_ACTION(action));
    INIT_FRM_BINDING(f, VAL_ACTION_BINDING(action));

    // The function did not actually execute, so no SPC(f) was never handed
    // out...the varlist should never have gotten managed.  So this context
    // can theoretically just be put back into the reuse list, or managed
    // and handed out for other purposes by the caller.
    //
    assert(NOT_SERIES_FLAG(f->varlist, MANAGED));

    // At the moment, Begin_Prefix_Action() marks the frame as having been
    // invoked...but since it didn't get managed it drops the flag in
    // Drop_Action().
    //
    // !!! The flag is new, as a gambit to try and avoid copying frames for
    // DO-ing just in order to expire the old identity.  Under development.
    //
    assert(NOT_ARRAY_FLAG(f->varlist, FRAME_HAS_BEEN_INVOKED));

    return threw;
}


//
//  Make_Frame_From_Varargs_Throws: C
//
// Routines like MATCH or DOES are willing to do impromptu specializations
// from a feed of instructions, so that a frame for an ACTION! can be made
// without actually running it yet.  This is also exposed by MAKE ACTION!.
//
// This pre-manages the exemplar, because it has to be done specially (it gets
// "stolen" out from under an evaluator's REBFRM*, and was manually tracked
// but never in the manual series list.)
//
bool Make_Frame_From_Varargs_Throws(
    REBVAL *out,
    const REBVAL *specializee,
    const REBVAL *varargs
){
    // !!! The vararg's frame is not really a parent, but try to stay
    // consistent with the naming in subframe code copy/pasted for now...
    //
    REBFRM *parent;
    if (not Is_Frame_Style_Varargs_May_Fail(&parent, varargs))
        fail (
            "Currently MAKE FRAME! on a VARARGS! only works with a varargs"
            " which is tied to an existing, running frame--not one that is"
            " being simulated from a BLOCK! (e.g. MAKE VARARGS! [...])"
        );

    assert(Is_Action_Frame(parent));

    // REBFRM whose built FRAME! context we will steal

    DECLARE_FRAME (f, parent->feed, EVAL_MASK_DEFAULT);
    Push_Frame(out, f);

    if (Get_If_Word_Or_Path_Throws(
        out,
        specializee,
        SPECIFIED,
        true  // push_refinements = true (DECLARE_FRAME captured original DSP)
    )){
        Drop_Frame(f);
        return true;
    }

    if (not IS_ACTION(out))
        fail (specializee);

    option(const REBSTR*) label = VAL_ACTION_LABEL(out);

    DECLARE_LOCAL (action);
    Move_Value(action, out);
    PUSH_GC_GUARD(action);

    // We interpret phrasings like `x: does all [...]` to mean something
    // like `x: specialize :all [block: [...]]`.  While this originated
    // from the Rebmu code golfing language to eliminate a pair of bracket
    // characters from `x: does [all [...]]`, it actually has different
    // semantics...which can be useful in their own right, plus the
    // resulting function will run faster.

    if (Make_Invocation_Frame_Throws(f, action)) {
        DROP_GC_GUARD(action);
        return true;
    }

    REBACT *act = VAL_ACTION(action);

    assert(NOT_SERIES_FLAG(f->varlist, MANAGED)); // not invoked yet
    assert(FRM_BINDING(f) == VAL_ACTION_BINDING(action));

    REBCTX *exemplar = Steal_Context_Vars(
        CTX(f->varlist),
        ACT_KEYLIST(act)
    );
    assert(ACT_NUM_PARAMS(act) == CTX_LEN(exemplar));

    INIT_LINK_KEYSOURCE(CTX_VARLIST(exemplar), ACT_KEYLIST(act));

    SET_SERIES_FLAG(f->varlist, MANAGED); // is inaccessible
    f->varlist = nullptr; // just let it GC, for now

    // May not be at end or thrown, e.g. (x: does just y x = 'y)
    //
    DROP_GC_GUARD(action);  // before drop to balance at right time
    Drop_Frame(f);

    // The exemplar may or may not be managed as of yet.  We want it
    // managed, but Push_Action() does not use ordinary series creation to
    // make its nodes, so manual ones don't wind up in the tracking list.
    //
    SET_SERIES_FLAG(CTX_VARLIST(exemplar), MANAGED);  // can't Manage_Series()

    Init_Frame(out, exemplar, label);
    return false;
}


// The REFRAMER native specializes out the FRAME! argument of the function
// being modified when it builds the interface.
//
// So the next thing to do is to fulfill the next function's frame without
// running it, in order to build a frame to put into that specialized slot.
// Then we run the reframer.
//
// !!! As a first cut we build on top of specialize, and look for the
// parameter by means of a particular labeled void.
//
REB_R Reframer_Dispatcher(REBFRM *f)
{
    REBARR *details = ACT_DETAILS(FRM_PHASE(f));
    assert(ARR_LEN(details) == IDX_REFRAMER_MAX);

    REBVAL* shim = DETAILS_AT(details, IDX_REFRAMER_SHIM);
    assert(IS_ACTION(shim));

    REBVAL* param_index = DETAILS_AT(details, IDX_REFRAMER_PARAM_INDEX);
    assert(IS_INTEGER(param_index));

    if (IS_END(f_value) or not (IS_WORD(f_value) or IS_PATH(f_value)))
        fail ("REFRAMER can only currently run on subsequent WORD!/PATH!");

    // First run ahead and make the frame we want from the feed.  We push
    // the frame so that we can fold the refinements used into it, without
    // needing to create an intermediate specialized function in the process.
    //
    // Note: We do not overwrite f->out in case of invisibility.

    DECLARE_FRAME (sub, f->feed, EVAL_MASK_DEFAULT);
    Push_Frame(f_spare, sub);

    if (Get_If_Word_Or_Path_Throws(
        sub->out,  // e.g. f_spare
        f_value,
        f_specifier,
        true  // push_refinements = true (DECLARE_FRAME captured original DSP)
    )){
        Drop_Frame(sub);
        return R_THROWN;
    }

    if (not IS_ACTION(sub->out))
        fail (rebUnrelativize(f_value));

    Fetch_Next_Forget_Lookback(sub);  // now, onto the arguments...

    option(const REBSTR*) label = VAL_ACTION_LABEL(sub->out);

    DECLARE_LOCAL (action);
    Move_Value(action, sub->out);
    PUSH_GC_GUARD(action);

    if (Make_Invocation_Frame_Throws(sub, action)) {
        DROP_GC_GUARD(action);
        return R_THROWN;
    }

    REBARR *varlist = sub->varlist;
    assert(NOT_SERIES_FLAG(varlist, MANAGED));  // not invoked yet
    sub->varlist = nullptr;  // just let it GC, for now

    REBACT *act = VAL_ACTION(action);
    assert(FRM_BINDING(sub) == VAL_ACTION_BINDING(action));

    INIT_LINK_KEYSOURCE(varlist, ACT_KEYLIST(act));

    // May not be at end or thrown, e.g. (x: does just y x = 'y)
    //
    DROP_GC_GUARD(action);  // before drop to balance at right time
    Drop_Frame(sub);

    // The exemplar may or may not be managed as of yet.  We want it
    // managed, but Push_Action() does not use ordinary series creation to
    // make its nodes, so manual ones don't wind up in the tracking list.
    //
    SET_SERIES_FLAG(varlist, MANAGED); // can't use Manage_Series

    REBVAL *arg = FRM_ARG(f, VAL_INT32(param_index));
    Init_Frame(arg, CTX(varlist), label);

    INIT_FRM_PHASE(f, VAL_ACTION(shim));
    INIT_FRM_BINDING(f, VAL_ACTION_BINDING(shim));

    return R_REDO_CHECKED;  // the redo will use the updated phase & binding
}


//
//  reframer*: native [
//
//  {Make a function that manipulate other actions at the callsite}
//
//      return: [action!]
//      shim "The action that has a FRAME! argument to supply"
//          [action!]
//      /parameter "Which parameter of the shim gets given the FRAME!"
//          [word!]
//  ]
//
REBNATIVE(reframer_p)
{
    INCLUDE_PARAMS_OF_REFRAMER_P;

    REBACT *shim = VAL_ACTION(ARG(shim));
    option(const REBSYM*) label = VAL_ACTION_LABEL(ARG(shim));

    REBDSP dsp_orig = DSP;

    struct Reb_Binder binder;
    INIT_BINDER(&binder);
    REBCTX *exemplar = Make_Context_For_Action_Push_Partials(
        ARG(shim),
        dsp_orig,
        &binder
    );

    option(REBCTX*) error = nullptr;  // can't fail() with binder in effect

    REBLEN param_index = 0;

    if (DSP != dsp_orig) {
        error = Error_User("REFRAMER can't use partial specializions ATM");
        goto cleanup_binder;
    }

  blockscope {
    const REBKEY *key;
    const REBPAR *param;
    
    if (REF(parameter)) {
        const REBSYM *symbol = VAL_WORD_SYMBOL(ARG(parameter));
        param_index = Get_Binder_Index_Else_0(&binder, symbol);
        if (param_index == 0) {
            error = Error_No_Arg(label, symbol);
            goto cleanup_binder;
        }
        key = CTX_KEY(exemplar, param_index);
        param = cast_PAR(CTX_VAR(exemplar, param_index));
    }
    else {
        param = Last_Unspecialized_Param(&key, shim);
        param_index = param - ACT_PARAMS_HEAD(shim) + 1;
    }

    // Make sure the parameter is able to accept FRAME! arguments (the type
    // checking will ultimately use hte same slot we overwrite here!)
    //
    if (not TYPE_CHECK(param, REB_FRAME)) {
        DECLARE_LOCAL (label_word);
        if (label)
            Init_Word(label_word, unwrap(label));
        else
            Init_Blank(label_word);

        DECLARE_LOCAL (param_word);
        Init_Word(param_word, KEY_SYMBOL(key));

        error = Error_Expect_Arg_Raw(
            label_word,
            Datatype_From_Kind(REB_FRAME),
            param_word
        );
        goto cleanup_binder;
    }
  }

  cleanup_binder: {
    const REBKEY *tail;
    const REBKEY *key = ACT_KEYS(&tail, shim);
    const REBPAR *param = ACT_PARAMS_HEAD(shim);
    for (; key != tail; ++key, ++param) {
        if (Is_Param_Hidden(param))
            continue;

        const REBSYM *symbol = KEY_SYMBOL(key);
        REBLEN index = Remove_Binder_Index_Else_0(&binder, symbol);
        assert(index != 0);
        UNUSED(index);
    }

    SHUTDOWN_BINDER(&binder);

    if (error)  // once binder is cleaned up, safe to raise errors
        fail (unwrap(error));
  }

    // We need the dispatcher to be willing to start the reframing step even
    // though the frame to be processed isn't ready yet.  So we have to
    // specialize the argument with something that type checks.  It wants a
    // FRAME!, so temporarily fill it with the exemplar frame itself.
    //
    // !!! An expired frame would be better, or tweaking the argument so it
    // takes a void and giving it ~pending~; would make bugs more obvious.
    //
    REBVAL *var = CTX_VAR(exemplar, param_index);
    Move_Value(var, CTX_ARCHETYPE(exemplar));
    SET_CELL_FLAG(var, VAR_MARKED_HIDDEN);

    // Make action with enough space to store the implementation phase and
    // which parameter to fill with the *real* frame instance.
    //
    Manage_Series(CTX_VARLIST(exemplar));
    REBACT *reframer = Alloc_Action_From_Exemplar(
        exemplar,  // shim minus the frame argument
        &Reframer_Dispatcher,
        IDX_REFRAMER_MAX  // details array capacity => [shim, param_index]
    );

    REBARR *details = ACT_DETAILS(reframer);
    Move_Value(ARR_AT(details, IDX_REFRAMER_SHIM), ARG(shim));
    Init_Integer(ARR_AT(details, IDX_REFRAMER_PARAM_INDEX), param_index);

    return Init_Action(D_OUT, reframer, label, UNBOUND);
}
