//
//  File: %c-action.c
//  Summary: "Central Interpreter Evaluator"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012-2020 Ren-C Open Source Contributors
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information
//
// Licensed under the Lesser GPL, Version 3.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.gnu.org/licenses/lgpl-3.0.html
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This file contains Process_Action_Throws(), which does the work of calling
// functions in the evaluator.
//
//=//// NOTES /////////////////////////////////////////////////////////////=//
//
// * Process_Action_Throws() is LONG.  That is largely a purposeful choice.
//   Breaking it into functions would add overhead (in the debug build if not
//   also release builds) and prevent interesting tricks and optimizations.
//   It is separated into sections, and the invariants in each section are
//   made clear with comments and asserts.
//

#include "sys-core.h"



// The frame contains a "feed" whose ->value typically represents a "current"
// step in the feed.  But the evaluator is organized in a way that the
// notion of what is "current" can get out of sync with the feed.  An example
// would be when a SET-WORD! evaluates its right hand side, causing the feed
// to advance an arbitrary amount.
//
// So the frame has its own frame state for tracking the "current" position,
// and maintains the optional cache of what the fetched value of that is.
// These macros help make the code less ambiguous.
//
#undef f_value
#undef f_gotten
#define f_next f->feed->value
#define f_next_gotten f->feed->gotten

// In debug builds, the KIND_BYTE() calls enforce cell validity...but slow
// things down a little.  So we only use the checked version in the main
// switch statement.  This abbreviation is also shorter and more legible.
//
#define kind_current KIND_BYTE_UNCHECKED(v)


//=//// ARGUMENT LOOP MODES ///////////////////////////////////////////////=//
//
// f->special is kept in sync with either:
//
// * f->param to indicate ordinary argument fulfillment for all the relevant
//   args, refinements, and refinement args of the function.
//
// * some other pointer to an array of REBVAL which is the same length as the
//   argument list.  Any non-null values in that array should be used in lieu
//   of an ordinary argument...e.g. that argument has been "specialized".
//
// All the states can be incremented across the length of the frame.  This
// means `++f->special` can be done without checking for null values.
//
// Additionally, in the f->param state, f->special will never register as
// anything other than a parameter.  This can speed up some checks, such as
// where `IS_NULLED(f->special)` can only match the other cases.
//
// Done with macros for speed in the debug build (which does not inline).
// The name of the trigger condition is included since reinforcing what's true
// at the callsite is good to help understand the state.

#define SPECIAL_IS_PARAM_SO_UNSPECIALIZED \
    (f->special == f->param)

#define SPECIAL_IS_ARBITRARY_SO_SPECIALIZED \
    (f->special != f->param and f->special != f->arg)


#ifdef DEBUG_EXPIRED_LOOKBACK
    #define CURRENT_CHANGES_IF_FETCH_NEXT \
        (f->feed->stress != nullptr)
#else
    #define CURRENT_CHANGES_IF_FETCH_NEXT \
        (v == &f->feed->lookback)
#endif


inline static void Expire_Out_Cell_Unless_Invisible(REBFRM *f) {
    SET_CELL_FLAG(f->out, OUT_MARKED_STALE);
}


// When arguments are hard quoted or soft-quoted, they don't call into the
// evaluator to do it.  But they need to use the logic of the evaluator for
// noticing when to defer enfix:
//
//     foo: func [...] [
//          return lit 1 then ["this needs to be returned"]
//     ]
//
// If the first time the THEN was seen was not after the 1, but when the
// LIT ran, it would get deferred until after the RETURN.  This is not
// consistent with the pattern people expect.
//
// Returns TRUE if it set the flag.
//
bool Lookahead_To_Sync_Enfix_Defer_Flag(struct Reb_Feed *feed) {
    assert(NOT_FEED_FLAG(feed, DEFERRING_ENFIX));
    assert(not feed->gotten);

    CLEAR_FEED_FLAG(feed, NO_LOOKAHEAD);

    if (not IS_WORD(feed->value))
        return false;

    feed->gotten = Try_Lookup_Word(feed->value, feed->specifier);

    if (not feed->gotten or not IS_ACTION(feed->gotten))
        return false;

    if (NOT_ACTION_FLAG(VAL_ACTION(feed->gotten), ENFIXED))
        return false;

    if (GET_ACTION_FLAG(VAL_ACTION(feed->gotten), DEFERS_LOOKBACK))
        SET_FEED_FLAG(feed, DEFERRING_ENFIX);
    return true;
}


// The code for modal parameter handling has to be used for both enfix and
// normal parameters.  It's enough code to be worth factoring out vs. repeat.
//
static bool Handle_Modal_In_Out_Throws(REBFRM *f) {
    switch (VAL_TYPE(f->out)) {
      case REB_SYM_WORD:
      case REB_SYM_PATH:
        Getify(f->out);  // don't run @append or @append/only
        goto enable_modal;

      case REB_SYM_GROUP:
      case REB_SYM_BLOCK:
        Plainify(f->out);  // run GROUP!, pass block as-is
        goto enable_modal;

      default:
        goto skip_enable_modal;
    }

  enable_modal: {
    //
    // !!! We could (should?) pre-check the paramlists to
    // make sure users don't try and make a modal argument
    // not followed by a refinement.  That would cost
    // extra, but avoid the test on every call.
    //
    const RELVAL *enable = f->param + 1;
    if (
        IS_END(enable)
        or not TYPE_CHECK(enable, REB_TS_REFINEMENT)
    ){
        fail ("Refinement must follow modal parameter");
    }
    if (not Is_Typeset_Empty(enable))
        fail ("Modal refinement cannot take arguments");

    // Signal refinement as being in use.
    //
    Init_Sym_Word(DS_PUSH(), VAL_PARAM_CANON(enable));
  }

  skip_enable_modal:
    //
    // Because the possibility of needing to see the uneval'd
    // value existed, the parameter had to act quoted.  Eval.
    //
    if (Eval_Value_Maybe_End_Throws(f->arg, f->out, SPECIFIED)) {
        Move_Value(f->arg, f->out);
        return true;
    }

    // The modal parameter can test to see if an expression vaporized, e.g.
    // `@(comment "hi")` or `@()`, and handle that case.
    //
    if (IS_END(f->arg))
        Init_Endish_Nulled(f->arg);

    return false;
}


//
//  Process_Action_Maybe_Stale_Throws: C
//
bool Process_Action_Maybe_Stale_Throws(REBFRM * const f)
{
  #if !defined(NDEBUG)
    assert(f->original);  // set by Begin_Action()
    Do_Process_Action_Checks_Debug(f);
  #endif

    if (f->special == f->arg)  // !!! now, a bad way to say "type check"
        goto typecheck_then_dispatch;

  fulfill:

    assert(DSP >= f->dsp_orig);  // path processing may push REFINEMENT!s

    assert(NOT_EVAL_FLAG(f, DOING_PICKUPS));

    for (; NOT_END(f->param); ++f->param, ++f->arg, ++f->special) {

  //=//// CONTINUES (AT TOP SO GOTOS DO NOT CROSS INITIALIZATIONS /////////=//

        Prep_Cell(f->arg);
        goto fulfill_loop_body;  // optimized out

      continue_fulfilling:

        if (GET_EVAL_FLAG(f, DOING_PICKUPS)) {
            if (DSP != f->dsp_orig)
                goto next_pickup;

            f->param = END_NODE;  // don't need f->param in paramlist
            goto fulfill_and_any_pickups_done;
        }
        continue;

      skip_fulfilling_arg_for_now:  // the GC marks args up through f->arg...

        Init_Unreadable_Void(f->arg);  // ...so cell must have valid bits
        continue;

  //=//// ACTUAL LOOP BODY ////////////////////////////////////////////////=//

      fulfill_loop_body:

  //=//// NEVER-FULFILLED ARGUMENTS ///////////////////////////////////////=//

        // Parameters that are hidden from the public interface will never
        // come from argument fulfillment.  If there is an exemplar, they are
        // set from that, otherwise they are undefined.
        //
        if (Is_Param_Hidden(f->param)) {
            if (SPECIAL_IS_PARAM_SO_UNSPECIALIZED) {  // no exemplar
                Init_Void(f->arg, SYM_UNDEFINED);
                SET_CELL_FLAG(f->arg, ARG_MARKED_CHECKED);
            }
            else {
                // !!! Previously we assumed type checking was done at the
                // time of specialization.  This is in preparation for when
                // type checking might be an expensive proposition.  We might
                // check here without erroring, patching back the bit onto
                // `f->special`...which could contain type checking to this
                // file and not burden SPECIALIZE/etc.  But we wouldn't want
                // to raise an error unless the type check loop runs.
                //
                Move_Value(f->arg, f->special);
            }
            goto continue_fulfilling;
        }

  //=//// A /REFINEMENT ARG ///////////////////////////////////////////////=//

        // Refinements can be tricky because the "visitation order" of the
        // parameters while walking across the parameter array might not
        // match the "consumption order" of the expressions that need to
        // be fetched from the callsite.  For instance:
        //
        //     foo: func [a /b [integer!] /c [integer!]] [...]
        //
        //     foo/b/c 10 20 30
        //     foo/c/b 10 20 30
        //
        // The first PATH! pushes /B to the top of stack, with /C below.
        // The second PATH! pushes /C to the top of stack, with /B below
        //
        // If the refinements can be popped off the stack in the order
        // that they are encountered, then this can be done in one pass.
        // Otherwise a second pass is needed.  But it is accelerated by
        // storing the parameter indices to revisit in the binding of the
        // words (e.g. /B and /C above) on the data stack.

        if (TYPE_CHECK(f->param, REB_TS_REFINEMENT)) {
            assert(NOT_EVAL_FLAG(f, DOING_PICKUPS));  // jump lower

            if (SPECIAL_IS_PARAM_SO_UNSPECIALIZED)  // args from callsite
                goto unspecialized_refinement;  // most common case (?)

            // A non-checked SYM-WORD! with binding indicates a partial
            // refinement with parameter index that needs to be pushed
            // to top of stack, hence HIGHER priority for fulfilling
            // @ the callsite than any refinements added by a PATH!.
            //
            if (IS_SYM_WORD(f->special)) {
                REBLEN partial_index = VAL_WORD_INDEX(f->special);
                REBSTR *partial_canon = VAL_STORED_CANON(f->special);

                Init_Sym_Word(DS_PUSH(), partial_canon);
                INIT_BINDING(DS_TOP, f->varlist);
                INIT_WORD_INDEX(DS_TOP, partial_index);
            }
            else
                assert(Is_Void_With_Sym(f->special, SYM_UNDEFINED));

  //=//// UNSPECIALIZED REFINEMENT SLOT ///////////////////////////////////=//

    // We want to fulfill all normal parameters before any refinements
    // that take arguments.  Ren-C allows normal parameters *after* any
    // refinement, that are not "refinement arguments".  So a refinement
    // that takes an argument must always fulfill using "pickups".

          unspecialized_refinement: {

            REBVAL *ordered = DS_TOP;  // v-- #2258
            const REBSTR *param_canon = VAL_PARAM_CANON(f->param);

            for (; ordered != DS_AT(f->dsp_orig); --ordered) {
                if (VAL_STORED_CANON(ordered) != param_canon)
                    continue;

                REBLEN offset = f->arg - FRM_ARGS_HEAD(f);
                INIT_BINDING(ordered, f->varlist);
                INIT_WORD_INDEX(ordered, offset + 1);

                if (Is_Typeset_Empty(f->param)) {
                    //
                    // There's no argument, so we won't need to come back
                    // for this one.  But we did need to set its index
                    // so we knew it was valid (errors later if not set).
                    //
                    goto used_refinement;
                }

                goto skip_fulfilling_arg_for_now;
            }

            goto unused_refinement; }  // not in path, not specialized yet

          unused_refinement:  // Note: might get pushed by a later slot

            Init_Nulled(f->arg);  // null means refinement not used
            SET_CELL_FLAG(f->arg, ARG_MARKED_CHECKED);
            goto continue_fulfilling;

          used_refinement:  // can hit this on redo, copy its argument

            Init_Blackhole(f->arg);  // # means refinement used
            SET_CELL_FLAG(f->arg, ARG_MARKED_CHECKED);
            goto continue_fulfilling;
        }

  //=//// ARGUMENT FULFILLMENT ////////////////////////////////////////////=//

      fulfill_arg: ;  // semicolon needed--next statement is declaration

        Reb_Param_Class pclass = VAL_PARAM_CLASS(f->param);
        assert(pclass != REB_P_LOCAL);  // should have been handled by hidden

  //=//// HANDLE IF NEXT ARG IS IN OUT SLOT (e.g. ENFIX, CHAIN) ///////////=//

        if (GET_FEED_FLAG(f->feed, NEXT_ARG_FROM_OUT)) {
            CLEAR_FEED_FLAG(f->feed, NEXT_ARG_FROM_OUT);

            if (GET_CELL_FLAG(f->out, OUT_MARKED_STALE)) {
                //
                // Something like `lib/help left-lit` is allowed to work,
                // but if it were just `obj/int-value left-lit` then the
                // path evaluation won...but LEFT-LIT still gets run.
                // It appears it has nothing to its left, but since we
                // remembered what happened we can give an informative
                // error message vs. a perplexing one.
                //
                if (GET_EVAL_FLAG(f, DIDNT_LEFT_QUOTE_PATH))
                    fail (Error_Literal_Left_Path_Raw());

                // Seeing an END in the output slot could mean that there
                // was really "nothing" to the left, or it could be a
                // consequence of a frame being in an argument gathering
                // mode, e.g. the `+` here will perceive "nothing":
                //
                //     if + 2 [...]
                //
                // If an enfixed function finds it has a variadic in its
                // first slot, then nothing available on the left is o.k.
                // It means we have to put a VARARGS! in that argument
                // slot which will react with TRUE to TAIL?, so feed it
                // from the global empty array.
                //
                if (Is_Param_Variadic(f->param)) {
                    Init_Varargs_Untyped_Enfix(f->arg, END_NODE);
                    goto continue_fulfilling;
                }

                // The OUT_MARKED_STALE flag is also used by BAR! to keep
                // a result in f->out, so that the barrier doesn't destroy
                // data in cases like `(1 + 2 | comment "hi")` => 3, but
                // left enfix should treat that just like an end.

                Init_Endish_Nulled(f->arg);
                goto continue_fulfilling;
            }

            if (Is_Param_Variadic(f->param)) {
                //
                // Stow unevaluated cell into an array-form variadic, so
                // the user can do 0 or 1 TAKEs of it.
                //
                // !!! It be evaluated when they TAKE (it if it's an
                // evaluative arg), but not if they don't.  Should failing
                // to TAKE be seen as an error?  Failing to take first
                // gives out-of-order evaluation.
                //
                assert(NOT_END(f->out));
                Init_Varargs_Untyped_Enfix(f->arg, f->out);
            }
            else switch (pclass) {
              case REB_P_NORMAL:
                enfix_normal_handling:

                Move_Value(f->arg, f->out);
                if (GET_CELL_FLAG(f->out, UNEVALUATED))
                    SET_CELL_FLAG(f->arg, UNEVALUATED);
                break;

              case REB_P_HARD_QUOTE:
                if (not GET_CELL_FLAG(f->out, UNEVALUATED)) {
                    //
                    // This can happen e.g. with `x: 10 | x >- lit`.  We
                    // raise an error in this case, while still allowing
                    // `10 >- lit` to work, so people don't have to go
                    // out of their way rethinking operators if it could
                    // just work out for inert types.
                    //
                    fail (Error_Evaluative_Quote_Raw());
                }

                // Is_Param_Skippable() accounted for in pre-lookback

                Move_Value(f->arg, f->out);
                SET_CELL_FLAG(f->arg, UNEVALUATED);
                break;

              case REB_P_MODAL: {
                if (not GET_CELL_FLAG(f->out, UNEVALUATED))
                    goto enfix_normal_handling;

                if (Handle_Modal_In_Out_Throws(f))
                    goto abort_action;

                break; }

              case REB_P_SOFT_QUOTE:
                //
                // Note: This permits f->out to not carry the UNEVALUATED
                // flag--enfixed operations which have evaluations on
                // their left are treated as if they were in a GROUP!.
                // This is important to `1 + 2 ->- lib/* 3` being 9, while
                // also allowing `1 + x: ->- lib/default [...]` to work.

                if (IS_QUOTABLY_SOFT(f->out)) {
                    if (Eval_Value_Throws(f->arg, f->out, SPECIFIED)) {
                        Move_Value(f->out, f->arg);
                        goto abort_action;
                    }
                }
                else {
                    Move_Value(f->arg, f->out);
                    SET_CELL_FLAG(f->arg, UNEVALUATED);
                }
                break;

              default:
                assert(false);
            }

            // When we see `1 + 2 * 3`, when we're at the 2, we don't
            // want to let the * run yet.  So set a flag which says we
            // won't do lookahead that will be cleared when function
            // takes an argument *or* when a new expression starts.
            //
            // This effectively puts the enfix into a *single step defer*.
            //
            if (GET_EVAL_FLAG(f, RUNNING_ENFIX)) {
                assert(NOT_FEED_FLAG(f->feed, NO_LOOKAHEAD));
                if (
                    NOT_ACTION_FLAG(FRM_PHASE(f), POSTPONES_ENTIRELY)
                    and
                    NOT_ACTION_FLAG(FRM_PHASE(f), DEFERS_LOOKBACK)
                ){
                    SET_FEED_FLAG(f->feed, NO_LOOKAHEAD);
                }
            }

            // We are expiring the output cell here because we have "used up"
            // the output result.  We don't know at this moment if the
            // function going to behave invisibly.  If it does, then we have
            // to *un-expire* the enfix invisible flag (!)
            //
            Expire_Out_Cell_Unless_Invisible(f);

            goto continue_fulfilling;
        }

  //=//// NON-ENFIX VARIADIC ARG (doesn't consume anything *yet*) /////////=//

        // Evaluation argument "hook" parameters (marked in MAKE ACTION!
        // by a `[[]]` in the spec, and in FUNC by `<variadic>`).  They point
        // back to this call through a reified FRAME!, and are able to
        // consume additional arguments during the function run.
        //
        if (Is_Param_Variadic(f->param)) {
            Init_Varargs_Untyped_Normal(f->arg, f);
            goto continue_fulfilling;
        }

  //=//// AFTER THIS, PARAMS CONSUME FROM CALLSITE IF NOT APPLY ///////////=//

        // If this is a non-enfix action, we're at least at *second* slot:
        //
        //     1 + non-enfix-action <we-are-here> * 3
        //
        // That's enough to indicate we're not going to read this as
        // `(1 + non-enfix-action <we-are-here>) * 3`.  Contrast with the
        // zero-arity case:
        //
        //     >> two: does [2]
        //     >> 1 + two * 3
        //     == 9
        //
        // We don't get here to clear the flag, so it's `(1 + two) * 3`
        //
        // But if it's enfix, arg gathering could still be like:
        //
        //      1 + <we-are-here> * 3
        //
        // So it has to wait until -after- the callsite gather happens to
        // be assured it can delete the flag, to ensure that:
        //
        //      >> 1 + 2 * 3
        //      == 9
        //
        if (NOT_EVAL_FLAG(f, RUNNING_ENFIX))
            CLEAR_FEED_FLAG(f->feed, NO_LOOKAHEAD);

        // Once a deferred flag is set, it must be cleared during the
        // evaluation of the argument it was set for... OR the function
        // call has to end.  If we need to gather an argument when that
        // is happening, it means neither of those things are true, e.g.:
        //
        //     if 1 then [<bad>] [print "this is illegal"]
        //     if (1 then [<good>]) [print "but you can do this"]
        //
        // The situation also arises in multiple arity infix:
        //
        //     arity-3-op: func [a b c] [...]
        //
        //     1 arity-3-op 2 + 3 <ambiguous>
        //     1 arity-3-op (2 + 3) <unambiguous>
        //
        if (GET_FEED_FLAG(f->feed, DEFERRING_ENFIX))
            fail (Error_Ambiguous_Infix_Raw());

  //=//// ERROR ON END MARKER, BAR! IF APPLICABLE /////////////////////////=//

        if (IS_END(f_next)) {
            Init_Endish_Nulled(f->arg);
            goto continue_fulfilling;
        }

        switch (pclass) {

  //=//// REGULAR ARG-OR-REFINEMENT-ARG (consumes 1 EVALUATE's worth) /////=//

          case REB_P_NORMAL:
          normal_handling: {
            if (GET_FEED_FLAG(f->feed, BARRIER_HIT)) {
                Init_Endish_Nulled(f->arg);
                goto continue_fulfilling;
            }

            REBFLGS flags = EVAL_MASK_DEFAULT
                | EVAL_FLAG_FULFILLING_ARG;

            if (Eval_Step_In_Subframe_Throws(f->arg, f, flags)) {
                Move_Value(f->out, f->arg);
                goto abort_action;
            }

            if (IS_END(f->arg))
                Init_Endish_Nulled(f->arg);
            break; }

  //=//// HARD QUOTED ARG-OR-REFINEMENT-ARG ///////////////////////////////=//

          case REB_P_HARD_QUOTE:
            if (not Is_Param_Skippable(f->param))
                Literal_Next_In_Frame(f->arg, f);  // CELL_FLAG_UNEVALUATED
            else {
                if (not Typecheck_Including_Constraints(f->param, f_next)) {
                    assert(Is_Param_Endable(f->param));
                    Init_Endish_Nulled(f->arg);  // not EVAL_FLAG_BARRIER_HIT
                    SET_CELL_FLAG(f->arg, ARG_MARKED_CHECKED);
                    goto continue_fulfilling;
                }
                Literal_Next_In_Frame(f->arg, f);
                SET_CELL_FLAG(f->arg, ARG_MARKED_CHECKED);
                SET_CELL_FLAG(f->arg, UNEVALUATED);
            }

            // Have to account for enfix deferrals in cases like:
            //
            //     return lit 1 then (x => [x + 1])
            //
            Lookahead_To_Sync_Enfix_Defer_Flag(f->feed);

            if (GET_CELL_FLAG(f->arg, ARG_MARKED_CHECKED))
                goto continue_fulfilling;

            break;

  //=//// MODAL ARG  //////////////////////////////////////////////////////=//

          case REB_P_MODAL: {
            if (GET_FEED_FLAG(f->feed, BARRIER_HIT)) {
                Init_Endish_Nulled(f->arg);
                goto continue_fulfilling;
            }

            if (not ANY_SYM_KIND(VAL_TYPE(f_next)))  // not an @xxx
                goto normal_handling;  // acquire as a regular argument

            Literal_Next_In_Frame(f->out, f);  // f->value is read-only...
            if (Handle_Modal_In_Out_Throws(f))  // ...out so we can Unsymify()
                goto abort_action;

            Lookahead_To_Sync_Enfix_Defer_Flag(f->feed);
            break; }

  //=//// SOFT QUOTED ARG-OR-REFINEMENT-ARG  //////////////////////////////=//

    // Quotes from the right already "win" over quotes from the left, in
    // a case like `help left-quoter` where they point at teach other.
    // But there's also an issue where something sits between quoting
    // constructs like the `[x]` in between the `else` and `->`:
    //
    //     if condition [...] else [x] -> [...]
    //
    // Here the neutral [x] is meant to be a left argument to the lambda,
    // producing the effect of:
    //
    //     if condition [...] else ([x] -> [...])
    //
    // To get this effect, we need a different kind of deferment that
    // hops over a unit of material.  Soft quoting is unique in that it
    // means we can do that hop over exactly one unit without breaking
    // the evaluator mechanics of feeding one element at a time with
    // "no takebacks".
    //
    // First, we cache the quoted argument into the frame slot.  This is
    // the common case of what is desired.  But if we advance the feed and
    // notice a quoting enfix construct afterward looking left, we call
    // into a nested evaluator before finishing the operation.

          case REB_P_SOFT_QUOTE:
            Literal_Next_In_Frame(f->arg, f);  // CELL_FLAG_UNEVALUATED

            // See remarks on Lookahead_To_Sync_Enfix_Defer_Flag().  We
            // have to account for enfix deferrals in cases like:
            //
            //     return if false '[foo] else '[bar]
            //
            // Note that this quoting lookahead ("lookback?") is exempt
            // from the usual "no lookahead" rule while gathering enfix
            // arguments.  This supports `null then x -> [1] else [2]`,
            // being 2.  See details at:
            //
            // https://forum.rebol.info/t/1361
            //
            if (
                Lookahead_To_Sync_Enfix_Defer_Flag(f->feed) and
                GET_ACTION_FLAG(VAL_ACTION(f->feed->gotten), QUOTES_FIRST)
            ){
                // We need to defer and let the right hand quote that is
                // quoting leftward win.  We use ST_EVALUATOR_LOOKING_AHEAD
                // to jump into a subframe where subframe->out is the f->arg,
                // and it knows to get the arg from there.

                REBFLGS flags = EVAL_MASK_DEFAULT
                    | EVAL_FLAG_FULFILLING_ARG
                    | FLAG_STATE_BYTE(ST_EVALUATOR_LOOKING_AHEAD)
                    | EVAL_FLAG_INERT_OPTIMIZATION;

                DECLARE_FRAME (subframe, f->feed, flags);

                Push_Frame(f->arg, subframe);
                bool threw = Eval_Throws(subframe);
                Drop_Frame(subframe);

                if (threw) {
                    Move_Value(f->out, f->arg);
                    goto abort_action;
                }
            }
            else if (IS_QUOTABLY_SOFT(f->arg)) {
                //
                // We did not defer the quoted argument.  If the argument
                // is something like a GROUP!, GET-WORD!, or GET-PATH!...
                // it has to be evaluated.
                //
                Move_Value(f_spare, f->arg);
                if (Eval_Value_Throws(f->arg, f_spare, f_specifier)) {
                    Move_Value(f->out, f->arg);
                    goto abort_action;
                }
            }
            break;

          default:
            assert(false);
        }

        // If FEED_FLAG_NO_LOOKAHEAD was set going into the argument
        // gathering above, it should have been cleared or converted into
        // FEED_FLAG_DEFER_ENFIX.
        //
        //     1 + 2 * 3
        //           ^-- this deferred its chance, so 1 + 2 will complete
        //
        assert(NOT_FEED_FLAG(f->feed, NO_LOOKAHEAD));

        assert(pclass != REB_P_LOCAL);
        assert(NOT_EVAL_FLAG(f, FULLY_SPECIALIZED));

        goto continue_fulfilling;
    }

    assert(IS_END(f->arg));  // arg can otherwise point to any arg cell

    // There may have been refinements that were skipped because the
    // order of definition did not match the order of usage.  They were
    // left on the stack with a pointer to the `param` and `arg` after
    // them for later fulfillment.
    //
    // Note that there may be functions on the stack if this is the
    // second time through, and we were just jumping up to check the
    // parameters in response to a R_REDO_CHECKED; if so, skip this.
    //
    if (DSP != f->dsp_orig and IS_SYM_WORD(DS_TOP)) {

      next_pickup:

        assert(IS_SYM_WORD(DS_TOP));

        if (not IS_WORD_BOUND(DS_TOP)) {  // the loop didn't index it
            mutable_KIND3Q_BYTE(DS_TOP) = REB_WORD;
            mutable_HEART_BYTE(DS_TOP) = REB_WORD;
            fail (Error_Bad_Refine_Raw(DS_TOP));  // so duplicate or junk
        }

        // FRM_ARGS_HEAD offsets are 0-based, while index is 1-based.
        // But +1 is okay, because we want the slots after the refinement.
        //
        REBINT offset =
            VAL_WORD_INDEX(DS_TOP) - (f->arg - FRM_ARGS_HEAD(f)) - 1;
        f->param += offset;
        f->arg += offset;
        f->special += offset;

        assert(VAL_STORED_CANON(DS_TOP) == VAL_PARAM_CANON(f->param));
        assert(TYPE_CHECK(f->param, REB_TS_REFINEMENT));
        DS_DROP();

        if (Is_Typeset_Empty(f->param)) {  // no callsite arg, just drop
            if (DSP != f->dsp_orig)
                goto next_pickup;

            f->param = END_NODE;  // don't need f->param in paramlist
            goto fulfill_and_any_pickups_done;
        }

        assert(IS_UNREADABLE_DEBUG(f->arg) or IS_NULLED(f->arg));
        SET_EVAL_FLAG(f, DOING_PICKUPS);
        goto fulfill_arg;
    }

  fulfill_and_any_pickups_done:

    CLEAR_EVAL_FLAG(f, DOING_PICKUPS);  // reevaluate may set flag again
    assert(IS_END(f->param));  // signals !Is_Action_Frame_Fulfilling()

    if (GET_FEED_FLAG(f->feed, NEXT_ARG_FROM_OUT)) {
        if (GET_EVAL_FLAG(f, DIDNT_LEFT_QUOTE_PATH))
            fail (Error_Literal_Left_Path_Raw());
    }

    if (GET_EVAL_FLAG(f, FULFILL_ONLY)) {  // only fulfillment, no typecheck
        assert(GET_CELL_FLAG(f->out, OUT_MARKED_STALE));  // didn't touch out
        goto skip_output_check;
    }

  //=//// ACTION! ARGUMENTS NOW GATHERED, DO TYPECHECK PASS ///////////////=//

    // It might seem convenient to type check arguments while they are being
    // fulfilled vs. performing another loop.  But the semantics of the system
    // allows manipulation of arguments between fulfillment and execution, and
    // that could turn invalid arguments good or valid arguments bad.  Plus
    // if all the arguments are evaluated before any type checking, that puts
    // custom type checks inside the body of a function on equal footing with
    // any system-optimized type checking.
    //
    // So a second loop is required by the system's semantics.

  typecheck_then_dispatch:
    Expire_Out_Cell_Unless_Invisible(f);

    f->param = ACT_PARAMS_HEAD(FRM_PHASE(f));
    f->arg = FRM_ARGS_HEAD(f);

    for (; NOT_END(f->param); ++f->param, ++f->arg) {
        assert(NOT_END(f->arg));  // all END fulfilled as Init_Endish_Nulled()
/*        if (VAL_PARAM_CLASS(f->param) == REB_P_LOCAL) {
            if (not IS_VOID(f->arg) and not IS_ACTION(f->arg))  // !!! TEMP TO TRY BOOT
                fail ("locals must be void");
            SET_CELL_FLAG(f->arg, ARG_MARKED_CHECKED);
            continue;
        } */

        // We can't a-priori typecheck the variadic argument, since the values
        // aren't calculated until the function starts running.  Instead we
        // stamp this instance of the varargs with a way to reach back and
        // see the parameter type signature.
        //
        // The data feed is unchanged (can come from this frame, or another,
        // or just an array from MAKE VARARGS! of a BLOCK!)
        //
        if (Is_Param_Variadic(f->param)) {
            //
            // The types on the parameter are for the values fetched later.
            // Actual argument must be a VARARGS!
            //
            if (not IS_VARARGS(f->arg))
                fail (Error_Not_Varargs(f, f->param, VAL_TYPE(f->arg)));

            VAL_VARARGS_PHASE_NODE(f->arg) = NOD(FRM_PHASE(f));

            // Store the offset so that both the arg and param locations can
            // quickly be recovered, while using only a single slot in the
            // REBVAL.  Sign denotes whether the parameter was enfixed or not.
            //
            bool enfix = false;  // !!! how does enfix matter?
            VAL_VARARGS_SIGNED_PARAM_INDEX(f->arg) =
                enfix
                    ? -(f->arg - FRM_ARGS_HEAD(f) + 1)
                    : f->arg - FRM_ARGS_HEAD(f) + 1;

            SET_CELL_FLAG(f->arg, ARG_MARKED_CHECKED);
            continue;
        }

        // Refinements have a special rule beyond plain type checking, in that
        // they don't just want an ISSUE! or NULL, they want # or NULL.
        //
        if (TYPE_CHECK(f->param, REB_TS_REFINEMENT)) {
            if (
                GET_EVAL_FLAG(f, FULLY_SPECIALIZED)
                and Is_Void_With_Sym(f->arg, SYM_UNDEFINED)
            ){
                Init_Nulled(f->arg);
                SET_CELL_FLAG(f->arg, ARG_MARKED_CHECKED);
            }
            else if (NOT_CELL_FLAG(f->arg, ARG_MARKED_CHECKED))
                Typecheck_Refinement(f->param, f->arg);
            continue;
        }

        // !!! In GCC 9.3.0-10 at -O2 optimization level in the C++ build
        // this seemed to trigger:
        //
        //   error: array subscript 2 is outside array bounds
        //      of 'const char [9]'
        //
        // It points to the problem being at VAL_STRING_AT()'s line:
        //
        //     const REBSTR *s = VAL_STRING(v);
        //
        // Attempts to further isolate it down were made by deleting and
        // inlining bits of code until one low-level line would trigger it.
        // This led to seemingly unrelated declaration of an unused byte
        // variable being able to cause it or not.  It may be a compiler
        // optimization bug...in any cae, that warning is disabled for
        // now on this file.  Review.

        if (IS_ENDISH_NULLED(f->arg)) {
            //
            // Note: `1 + comment "foo"` => `1 +`, arg is END
            //
            if (not Is_Param_Endable(f->param))
                fail (Error_No_Arg(f, f->param));

            SET_CELL_FLAG(f->arg, ARG_MARKED_CHECKED);
            continue;
        }

        REBYTE kind_byte = KIND3Q_BYTE(f->arg);

        if (
            kind_byte == REB_BLANK  // v-- e.g. <blank> param
            and TYPE_CHECK(f->param, REB_TS_NOOP_IF_BLANK)
        ){
            SET_CELL_FLAG(f->arg, ARG_MARKED_CHECKED);
            SET_EVAL_FLAG(f, TYPECHECK_ONLY);
            continue;
        }

        // Apply constness if requested.
        //
        // !!! Should explicit mutability override, so people can say things
        // like `foo: func [...] mutable [...]` ?  This seems bad, because the
        // contract of the function hasn't been "tweaked" with reskinning.
        //
        if (TYPE_CHECK(f->param, REB_TS_CONST))
            SET_CELL_FLAG(f->arg, CONST);

        // !!! Review when # is used here
        if (TYPE_CHECK(f->param, REB_TS_REFINEMENT)) {
            Typecheck_Refinement(f->param, f->arg);
            continue;
        }

        if (VAL_PARAM_SYM(f->param) == SYM_RETURN)
            continue;  // !!! let whatever go for now

        if (not Typecheck_Including_Constraints(f->param, f->arg))
            fail (Error_Arg_Type(f, f->param, VAL_TYPE(f->arg)));

        SET_CELL_FLAG(f->arg, ARG_MARKED_CHECKED);
    }


  //=//// ACTION! ARGUMENTS NOW GATHERED, DISPATCH PHASE //////////////////=//

  dispatch:

    // This happens if you have something intending to act as enfix but
    // that does not consume arguments, e.g. `x: enfixed func [] []`.
    // An enfixed function with no arguments might sound dumb, but it allows
    // a 0-arity function to run in the same evaluation step as the left
    // hand side.  This is how expression work (see `|:`)
    //
    assert(NOT_EVAL_FLAG(f, UNDO_MARKED_STALE));
    if (GET_FEED_FLAG(f->feed, NEXT_ARG_FROM_OUT)) {
        assert(GET_EVAL_FLAG(f, RUNNING_ENFIX));
        CLEAR_FEED_FLAG(f->feed, NEXT_ARG_FROM_OUT);
        f->out->header.bits |= CELL_FLAG_OUT_MARKED_STALE;  // won't undo this
    }
    else if (GET_EVAL_FLAG(f, RUNNING_ENFIX) and NOT_END(f->out))
        SET_EVAL_FLAG(f, UNDO_MARKED_STALE);

    assert(IS_END(f->param));
    assert(
        IS_END(f_next)
        or FRM_IS_VARIADIC(f)
        or IS_VALUE_IN_ARRAY_DEBUG(f->feed->array, f_next)
    );

    if (GET_EVAL_FLAG(f, TYPECHECK_ONLY)) {  // <blank> uses this
        Init_Nulled(f->out);  // by convention: BLANK! in, NULL out
        goto skip_output_check;
    }

    f_next_gotten = nullptr;  // arbitrary code changes fetched variables

    // Note that the dispatcher may push ACTION! values to the data stack
    // which are used to process the return result after the switch.
    //
  blockscope {
    REBACT *phase = FRM_PHASE(f);

    // Native code trusts that type checking has ensured it won't get bits
    // in its argument slots that the C won't recognize.  Usermode code that
    // gets its hands on a native's FRAME! (e.g. for debug viewing) can't be
    // allowed to change the frame values to other bit patterns out from
    // under the C or it could result in a crash.  By making the IS_NATIVE
    // flag the same as the HOLD info bit, we can make sure the frame gets
    // marked protected if it's a native...without needing an if() branch.
    //
    STATIC_ASSERT(PARAMLIST_FLAG_IS_NATIVE == SERIES_INFO_HOLD);
    SER(f->varlist)->info.bits |= SER(phase)->header.bits & SERIES_INFO_HOLD;

    REBNAT dispatcher = ACT_DISPATCHER(phase);

    const REBVAL *r = (*dispatcher)(f);

    if (r == f->out) {
        //
        // common case; we'll want to clear the UNEVALUATED flag if it's
        // not an invisible return result (other cases Move_Value())
        //
    }
    else if (not r) {  // API and internal code can both return `nullptr`
        Init_Nulled(f->out);
        goto dispatch_completed;  // skips invisible check
    }
    else if (GET_CELL_FLAG(r, ROOT)) {  // API, from Alloc_Value()
        Handle_Api_Dispatcher_Result(f, r);
        goto dispatch_completed;  // skips invisible check
    }
    else switch (KIND3Q_BYTE(r)) {  // it's a "pseudotype" instruction
        //
        // !!! Thrown values used to be indicated with a bit on the value
        // itself, but now it's conveyed through a return value.  This
        // means typical return values don't have to run through a test
        // for if they're thrown or not, but it means Eval_Core has to
        // return a boolean to pass up the state.  It may not be much of
        // a performance win either way, but recovering the bit in the
        // values is a definite advantage--as header bits are scarce!
        //
      case REB_R_THROWN: {
        const REBVAL *label = VAL_THROWN_LABEL(f->out);
        if (IS_ACTION(label)) {
            if (
                VAL_ACTION(label) == NATIVE_ACT(unwind)
                and VAL_BINDING(label) == NOD(f->varlist)
            ){
                // Eval_Core catches unwinds to the current frame, so throws
                // where the "/name" is the JUMP native with a binding to
                // this frame, and the thrown value is the return code.
                //
                // !!! This might be a little more natural if the name of
                // the throw was a FRAME! value.  But that also would mean
                // throws named by frames couldn't be taken advantage by
                // the user for other features, while this only takes one
                // function away.
                //
                CATCH_THROWN(f->out, f->out);
                goto dispatch_completed;
            }
            else if (
                VAL_ACTION(label) == NATIVE_ACT(redo)
                and VAL_BINDING(label) == NOD(f->varlist)
            ){
                // This was issued by REDO, and should be a FRAME! with
                // the phase and binding we are to resume with.
                //
                CATCH_THROWN(f->out, f->out);
                assert(IS_FRAME(f->out));

                // We are reusing the frame and may be jumping to an
                // "earlier phase" of a composite function, or even to
                // a "not-even-earlier-just-compatible" phase of another
                // function.  Type checking is necessary, as is zeroing
                // out any locals...but if we're jumping to any higher
                // or different phase we need to reset the specialization
                // values as well.
                //
                // Since dispatchers run arbitrary code to pick how (and
                // if) they want to change the phase on each redo, we
                // have no easy way to tell if a phase is "earlier" or
                // "later".
                //
                // !!! Consider folding this pass into an option for the
                // typechecking loop itself.
                //
                REBACT *redo_phase = VAL_PHASE_ELSE_ARCHETYPE(f->out);
                f->param = ACT_PARAMS_HEAD(redo_phase);
                f->special = ACT_SPECIALTY_HEAD(redo_phase);
                f->arg = FRM_ARGS_HEAD(f);
                for (; NOT_END(f->param); ++f->param, ++f->arg, ++f->special) {
                    switch (VAL_PARAM_CLASS(f->param)) {
                      case REB_P_LOCAL:
                        Init_Void(f->arg, SYM_UNDEFINED);
                        break;
                      
                      case REB_P_SEALED:
                        if (f->param == f->special)  // sealed local
                            Init_Void(f->arg, SYM_UNDEFINED);
                        else
                            Blit_Specific(f->arg, f->special);
                        break;

                      case REB_P_SPECIALIZED:
                        assert(f->param != f->special);
                        Blit_Specific(f->arg, f->special);
                        break;

                      default:
                        break;
                    }
                }

                INIT_FRM_PHASE(f, redo_phase);
                FRM_BINDING(f) = VAL_BINDING(f->out);
                CLEAR_EVAL_FLAG(f, UNDO_MARKED_STALE);
                goto typecheck_then_dispatch;
            }
        }

        // Stay THROWN and let stack levels above try and catch
        //
        goto abort_action; }

      case REB_R_REDO:
        //
        // This instruction represents the idea that it is desired to
        // run the f->phase again.  The dispatcher may have changed the
        // value of what f->phase is, for instance.

        CLEAR_EVAL_FLAG(f, UNDO_MARKED_STALE);

        if (not EXTRA(Any, r).flag)  // R_REDO_UNCHECKED
            goto dispatch;

        goto typecheck_then_dispatch;

      default:
        assert(!"Invalid pseudotype returned from action dispatcher");
    }
  }

  //=//// CHECK FOR INVISIBILITY (STALE OUTPUT) ///////////////////////////=//

    if (not (f->out->header.bits & CELL_FLAG_OUT_MARKED_STALE))
        CLEAR_CELL_FLAG(f->out, UNEVALUATED);
    else {
        // We didn't know before we ran the enfix function if it was going
        // to be invisible, so the output was expired.  Un-expire it if we
        // are supposed to do so.
        //
        STATIC_ASSERT(
            EVAL_FLAG_UNDO_MARKED_STALE == CELL_FLAG_OUT_MARKED_STALE
        );
        f->out->header.bits ^= (f->flags.bits & EVAL_FLAG_UNDO_MARKED_STALE);

        // If a "good" output is in `f->out`, the invisible should have
        // had no effect on it.  So jump to the position after output
        // would be checked by a normal function.
        //
        if (NOT_CELL_FLAG(f->out, OUT_MARKED_STALE) or IS_END(f_next)) {
            //
            // Note: could be an END that is not "stale", example:
            //
            //     is-barrier?: func [x [<end> integer!]] [null? x]
            //     is-barrier? (<| 10)
            //
            goto dispatch_completed;
        }

        // If the evaluation is being called by something like EVALUATE,
        // they may want to see the next value literally.  Refer to this
        // explanation:
        //
        // https://forum.rebol.info/t/1173/4
        //
        // But argument evaluation isn't customizable at that level, and
        // wants all the invisibles processed.  So only do one-at-a-time
        // invisibles if we're not fulfilling arguments.
        //
        if (GET_EVAL_FLAG(f, FULFILLING_ARG))
            goto dispatch_completed;

        // Note that we do not do START_NEW_EXPRESSION() here when an
        // invisible is being processed as part of an argument.  They
        // all get lumped into one step.
        //
        // !!! How does this interact with the idea of a debugger that could
        // single step across invisibles (?)  Is that only a "step in", as
        // one would have to do when dealing with a function argument?
        //
        assert(NOT_EVAL_FLAG(f, FULFILL_ONLY));
        Drop_Action(f);
        return false;
    }

  dispatch_completed:

  //=//// ACTION! CALL COMPLETION /////////////////////////////////////////=//

    // Here we know the function finished and nothing threw past it or
    // FAIL / fail()'d.  It should still be in REB_ACTION evaluation
    // type, and overwritten the f->out with a non-thrown value.  If the
    // function composition is a CHAIN, the chained functions are still
    // pending on the stack to be run.

  #if !defined(NDEBUG)
    Do_After_Action_Checks_Debug(f);
  #endif

  skip_output_check:

    CLEAR_EVAL_FLAG(f, UNDO_MARKED_STALE);

    // If we have functions pending to run on the outputs (e.g. this was
    // the result of a CHAIN) we can run those chained functions in the
    // same REBFRM, for efficiency.
    //
    while (DSP != f->dsp_orig) {
        //
        // We want to keep the label that the function was invoked with,
        // because the other phases in the chain are implementation
        // details...and if there's an error, it should still show the
        // name the user invoked the function with.  But we have to drop
        // the action args, as the paramlist is likely be completely
        // incompatible with this next chain step.
        //
        Drop_Action(f);
        Push_Action(f, VAL_ACTION(DS_TOP), VAL_BINDING(DS_TOP));

        // We use the same mechanism as enfix operations do...give the
        // next chain step its first argument coming from f->out
        //
        // !!! One side effect of this is that unless CHAIN is changed
        // to check, your chains can consume more than one argument.
        // This might be interesting or it might be bugs waiting to
        // happen, trying it out of curiosity for now.
        //
        Begin_Prefix_Action(f, VAL_ACTION_LABEL(DS_TOP));
        assert(NOT_FEED_FLAG(f->feed, NEXT_ARG_FROM_OUT));
        SET_FEED_FLAG(f->feed, NEXT_ARG_FROM_OUT);

        DS_DROP();

        goto fulfill;
    }

    Drop_Action(f);

    // Want to keep this flag between an operation and an ensuing enfix in
    // the same frame, so can't clear in Drop_Action(), e.g. due to:
    //
    //     left-lit: enfix :lit
    //     o: make object! [f: does [1]]
    //     o/f left-lit  ; want error suggesting -> here, need flag for that
    //
    CLEAR_EVAL_FLAG(f, DIDNT_LEFT_QUOTE_PATH);
    assert(NOT_FEED_FLAG(f->feed, NEXT_ARG_FROM_OUT));  // must be consumed

    return false;  // false => not thrown

  abort_action:

    Drop_Action(f);
    DS_DROP_TO(f->dsp_orig);  // drop unprocessed refinements/chains on stack

    return true;  // true => thrown
}
