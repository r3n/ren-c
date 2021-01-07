//
//  File: %c-reframer.c
//  Summary: "Function that can transform arbitrary callsite functions"
//  Section: datatypes
//  Project: "Ren-C Language Interpreter and Run-time Environment"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2020 Ren-C Open Source Contributors
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

    const REBSTR *label = VAL_ACTION_LABEL(sub->out);

    DECLARE_LOCAL (action);
    Move_Value(action, sub->out);
    PUSH_GC_GUARD(action);

    REBVAL *first_arg;
    if (Make_Invocation_Frame_Throws(sub, &first_arg, action)) {
        DROP_GC_GUARD(action);
        return R_THROWN;
    }

    UNUSED(first_arg); // MATCH uses to get its answer faster, we don't need

    REBACT *act = VAL_ACTION(action);

    assert(NOT_SERIES_FLAG(sub->varlist, MANAGED)); // not invoked yet
    assert(FRM_BINDING(sub) == VAL_ACTION_BINDING(action));

    REBCTX *stolen = Steal_Context_Vars(
        CTX(sub->varlist),
        NOD(ACT_KEYLIST(act))
    );
    assert(ACT_NUM_PARAMS(act) == CTX_LEN(stolen));

    INIT_LINK_KEYSOURCE(CTX_VARLIST(stolen), NOD(ACT_KEYLIST(act)));

    SET_SERIES_FLAG(sub->varlist, MANAGED); // is inaccessible
    sub->varlist = nullptr; // just let it GC, for now

    // May not be at end or thrown, e.g. (x: does just y x = 'y)
    //
    DROP_GC_GUARD(action);  // before drop to balance at right time
    Drop_Frame(sub);

    // The exemplar may or may not be managed as of yet.  We want it
    // managed, but Push_Action() does not use ordinary series creation to
    // make its nodes, so manual ones don't wind up in the tracking list.
    //
    SET_SERIES_FLAG(CTX_VARLIST(stolen), MANAGED); // can't use Manage_Series

    REBVAL *arg = FRM_ARG(f, VAL_INT32(param_index));
    Init_Frame(arg, stolen, label);

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
    option(const REBSTR*) label = VAL_ACTION_LABEL(ARG(shim));

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
    REBVAL *param;
    
    if (REF(parameter)) {
        const REBSTR *spelling = VAL_WORD_SPELLING(ARG(parameter));
        param_index = Get_Binder_Index_Else_0(&binder, spelling);
        if (param_index == 0) {
            error = Error_No_Arg(label, spelling);
            goto cleanup_binder;
        }
        key = CTX_KEY(exemplar, param_index);
        param = CTX_VAR(exemplar, param_index);
    }
    else {
        param = Last_Unspecialized_Param(&key, shim);
        param_index = param - ACT_SPECIALTY_HEAD(shim) + 1;
    }

    // Make sure the parameter is able to accept FRAME! arguments (the type
    // checking will ultimately use hte same slot we overwrite here!)
    //
    if (not TYPE_CHECK(param, REB_FRAME)) {
        DECLARE_LOCAL (label_word);
        if (label)
            Init_Word(label_word, label);
        else
            Init_Blank(label_word);

        DECLARE_LOCAL (param_word);
        Init_Word(param_word, KEY_SPELLING(key));

        error = Error_Expect_Arg_Raw(
            label_word,
            Datatype_From_Kind(REB_FRAME),
            param_word
        );
        goto cleanup_binder;
    }
  }

  cleanup_binder: {
    const REBKEY *key = ACT_KEYS_HEAD(shim);
    REBVAL *special = ACT_SPECIALTY_HEAD(shim);
    for (; NOT_END(key); ++key, ++special) {
        if (Is_Param_Hidden(special))
            continue;

        const REBSTR *spelling = KEY_SPELLING(key);
        REBLEN index = Remove_Binder_Index_Else_0(&binder, spelling);
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
    SET_CELL_FLAG(var, ARG_MARKED_CHECKED);

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
