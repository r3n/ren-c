//
//  File: %c-specialize.c
//  Summary: "Routines for Creating Function Variations with Fixed Parameters"
//  Section: datatypes
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2015-2020 Ren-C Open Source Contributors
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
// A specialization is an ACTION! which has some of its parameters fixed.
// e.g. `ap10: specialize :append [value: 5 + 5]` makes ap10 have all the same
// refinements available as APPEND, but otherwise just takes one series arg,
// as it will always be appending 10.
//
// The method used is to store a FRAME! in the specialization's ACT_DETAILS().
// Frame cells which carry the ARG_MARKED_CHECKED bit are considered to be
// specialized out.
//
// !!! Future directions would make use of unused frame cells to store the
// information for gathering the argument, e.g. its parameter modality and
// its type..
//
// The code for partial specialization is unified with the code for full
// specialization, because while `:append/part` doesn't fulfill a frame slot,
// `:append/only` does.  The code for this is still being designed.
//

#include "sys-core.h"

// Originally, the ACT_DETAILS() of a paramlist held the partially or fully
// filled FRAME! to be executed.  However, it turned out that since a
// specialization's exemplar is available through ACT_SPECIALTY(), there's
// no need to have a redundant copy.  This means a specialization can use
// the compact singular array form for ACT_DETAILS()
//
enum {
    IDX_SPECIALIZER_MAX = 1  // has just ACT_DETAILS[0], the ACT_ARCHETYPE()
};


//
//  Specializer_Dispatcher: C
//
// The evaluator does not do any special "running" of a specialized frame.
// All of the contribution that the specialization had to make was taken care
// of when Eval_Core() used f->special to fill from the exemplar.  So all this
// does is change the phase and binding to match the function this layer was
// specializing.
//
REB_R Specializer_Dispatcher(REBFRM *f)
{
    REBARR *details = ACT_DETAILS(FRM_PHASE(f));
    assert(ARR_LEN(details) == IDX_SPECIALIZER_MAX);  // just archetype!
    UNUSED(details);

    REBCTX *exemplar = ACT_EXEMPLAR(FRM_PHASE(f));

    INIT_FRM_PHASE(f, CTX_FRAME_ACTION(exemplar));
    INIT_FRM_BINDING(f, CTX_FRAME_BINDING(exemplar));

    return R_REDO_UNCHECKED; // redo uses the updated phase and binding
}


//
//  Make_Context_For_Action_Push_Partials: C
//
// This creates a FRAME! context with `~unset~` cells in the unspecialized
// slots.  The reason this is chosen instead of NULL is that specialization
// with NULL is frequent, and this takes only *one* void state away.  To
// actually specialize with `~unset~` one must either write a temporary
// value and then adapt over it, or use the PROTECT/HIDE mechanic to hide
// the field from view (thus making it presumed specialized).
//
// For partial refinement specializations in the action, this will push the
// refinement to the stack.  In this way it retains the ordering information
// implicit in the partial refinements of an action's existing specialization.
//
// It is able to take in more specialized refinements on the stack.  These
// will be ordered *after* partial specializations in the function already.
// The caller passes in the stack pointer of the lowest priority refinement,
// which goes up to DSP for the highest of those added specializations.
//
// Since this is walking the parameters to make the frame already--and since
// we don't want to bind to anything specialized out (including the ad-hoc
// refinements added on the stack) we go ahead and collect bindings from the
// frame if needed.
//
REBCTX *Make_Context_For_Action_Push_Partials(
    const REBVAL *action,  // need ->binding, so can't just be a REBACT*
    REBDSP lowest_ordered_dsp,  // caller can add refinement specializations
    option(struct Reb_Binder*) binder
){
    REBDSP highest_ordered_dsp = DSP;

    REBACT *act = VAL_ACTION(action);

    REBLEN num_slots = ACT_NUM_PARAMS(act) + 1;  // +1 is for CTX_ARCHETYPE()
    REBARR *varlist = Make_Array_Core(num_slots, SERIES_MASK_VARLIST);
    INIT_CTX_KEYLIST_SHARED(CTX(varlist), ACT_PARAMLIST(act));

    RELVAL *rootvar = ARR_HEAD(varlist);
    INIT_VAL_FRAME_ROOTVAR(
        rootvar,
        varlist,
        VAL_ACTION(action),
        VAL_ACTION_BINDING(action)
    );

    const REBVAL *param = ACT_PARAMS_HEAD(act);
    REBVAL *arg = SPECIFIC(rootvar) + 1;

    // If there is a PARTIALS list, then push its refinements.
    //
    REBARR *specialty = ACT_SPECIALTY(act);
    if (GET_ARRAY_FLAG(specialty, IS_PARTIALS)) {
        const REBVAL *word = SPECIFIC(ARR_HEAD(specialty));
        for (; NOT_END(word); ++word)
            Move_Value(DS_PUSH(), word);
    }

    const REBVAL *special = ACT_SPECIALTY_HEAD(act);  // of exemplar/paramlist

    REBLEN index = 1; // used to bind REFINEMENT! values to parameter slots

    REBCTX *exemplar = ACT_EXEMPLAR(act); // may be null
    if (exemplar)
        assert(special == CTX_VARS_HEAD(exemplar));
    else
        assert(special == ACT_PARAMS_HEAD(act));

    for (; NOT_END(param); ++param, ++arg, ++special, ++index) {
        Prep_Cell(arg);

        if (Is_Param_Hidden(param, special)) {  // local or specialized
            if (param == special) {  // no prior exemplar
                Init_Void(arg, SYM_UNSET);
                SET_CELL_FLAG(arg, ARG_MARKED_CHECKED);
            }
            else
                Blit_Specific(arg, special);  // preserve ARG_MARKED_CHECKED

          continue_specialized:

            continue;
        }

        assert(NOT_CELL_FLAG(special, ARG_MARKED_CHECKED));

        const REBSTR *symbol = VAL_PARAM_SPELLING(param);  // added to binding
        if (not TYPE_CHECK(param, REB_TS_REFINEMENT)) {  // nothing to push

          continue_unspecialized:

            Init_Void(arg, SYM_UNSET);  // *not* ARG_MARKED_CHECKED
            if (binder)
                Add_Binder_Index(unwrap(binder), symbol, index);

            continue;
        }

        // Unspecialized refinement slot.  It may be partially specialized,
        // e.g. we may have pushed to the stack from the PARTIALS for it.
        //
        // !!! If partials were allowed to encompass things like /ONLY then
        // we would have to use that to fill the slot here.  For the moment,
        // a full new exemplar is generated for parameterless refinements
        // which seems expensive for the likes of :append/only, when we
        // can make :append/dup more compactly.  Rethink.

        assert(
            (special == param and IS_PARAM(special))
            or Is_Void_With_Sym(special, SYM_UNSET)
        );

        // Check the passed-in refinements on the stack for usage.
        //
        REBDSP dsp = highest_ordered_dsp;
        for (; dsp != lowest_ordered_dsp; --dsp) {
            STKVAL(*) ordered = DS_AT(dsp);
            if (VAL_WORD_SPELLING(ordered) != symbol)
                continue;  // just continuing this loop

            assert(not IS_WORD_BOUND(ordered));  // we bind only one
            INIT_VAL_WORD_BINDING(ordered, varlist);
            INIT_VAL_WORD_PRIMARY_INDEX(ordered, index);

            if (not Is_Typeset_Empty(param))  // needs argument
                goto continue_unspecialized;

            // If refinement named on stack takes no arguments, then it can't
            // be partially specialized...only fully, and won't be bound:
            //
            //     specialize :append/only [only: #]  ; only not bound
            //
            Init_Blackhole(arg);
            SET_CELL_FLAG(arg, ARG_MARKED_CHECKED);
            goto continue_specialized;
        }

        goto continue_unspecialized;
    }

    TERM_ARRAY_LEN(varlist, num_slots);
    MISC_META_NODE(varlist) = nullptr;  // GC sees this, we must initialize

    return CTX(varlist);
}


//
//  Make_Context_For_Action: C
//
// !!! The ultimate concept is that it would be possible for a FRAME! to
// preserve ordering information such that an ACTION! could be made from it.
// Right now the information is the stack ordering numbers of the refinements
// which to make it usable should be relative to the lowest ordered DSP and
// not absolute.
//
REBCTX *Make_Context_For_Action(
    const REBVAL *action, // need ->binding, so can't just be a REBACT*
    REBDSP lowest_ordered_dsp,
    option(struct Reb_Binder*) binder
){
    REBCTX *exemplar = Make_Context_For_Action_Push_Partials(
        action,
        lowest_ordered_dsp,
        binder
    );

    Manage_Series(CTX_VARLIST(exemplar));  // !!! was needed before, review
    DS_DROP_TO(lowest_ordered_dsp);
    return exemplar;
}


//
//  Specialize_Action_Throws: C
//
// Create a new ACTION! value that uses the same implementation as another,
// but just takes fewer arguments or refinements.  It does this by storing a
// heap-based "exemplar" FRAME! in the specialized action; this stores the
// values to preload in the stack frame cells when it is invoked.
//
// The caller may provide information on the order in which refinements are
// to be specialized, using the data stack.  These refinements should be
// pushed in the *reverse* order of their invocation, so append/dup/part
// has /DUP at DS_TOP, and /PART under it.  List stops at lowest_ordered_dsp.
//
bool Specialize_Action_Throws(
    REBVAL *out,
    REBVAL *specializee,
    option(REBVAL*) def,  // !!! REVIEW: binding modified directly, not copied
    REBDSP lowest_ordered_dsp
){
    assert(out != specializee);

    struct Reb_Binder binder;
    if (def)
        INIT_BINDER(&binder);

    REBACT *unspecialized = VAL_ACTION(specializee);

    // This produces a context where partially specialized refinement slots
    // will be on the stack (including any we are adding "virtually", from
    // the current DSP down to the lowest_ordered_dsp).
    //
    // All unspecialized slots (including partials) will be ~unset~
    //
    REBCTX *exemplar = Make_Context_For_Action_Push_Partials(
        specializee,
        lowest_ordered_dsp,
        def ? &binder : nullptr
    );
    Manage_Series(CTX_VARLIST(exemplar)); // destined to be managed, guarded

    if (def) { // code that fills the frame...fully or partially
        //
        // Bind all the SET-WORD! in the body that match params in the frame
        // into the frame.  This means `value: value` can very likely have
        // `value:` bound for assignments into the frame while `value` refers
        // to whatever value was in the context the specialization is running
        // in, but this is likely the more useful behavior.
        //
        Virtual_Bind_Deep_To_Existing_Context(
            unwrap(def),
            exemplar,
            &binder,
            REB_SET_WORD
        );

        // !!! Only one binder can be in effect, and we're calling arbitrary
        // code.  Must clean up now vs. in loop we do at the end.  :-(
        //
        RELVAL *key = CTX_KEYS_HEAD(exemplar);
        REBVAL *var = CTX_VARS_HEAD(exemplar);
        for (; NOT_END(key); ++key, ++var) {
            if (Is_Param_Hidden(key, var))
                continue;  // maybe refinement from stack, now specialized out

            Remove_Binder_Index(&binder, VAL_KEY_SPELLING(key));
        }
        SHUTDOWN_BINDER(&binder);

        // Run block and ignore result (unless it is thrown)
        //
        PUSH_GC_GUARD(exemplar);
        bool threw = Do_Any_Array_At_Throws(out, unwrap(def), SPECIFIED);
        DROP_GC_GUARD(exemplar);

        if (threw) {
            DS_DROP_TO(lowest_ordered_dsp);
            return true;
        }
    }

    REBARR *paramlist = ACT_PARAMLIST(unspecialized);

    const RELVAL *param = ARR_AT(paramlist, 1);
    REBVAL *arg = CTX_VARS_HEAD(exemplar);

    REBDSP ordered_dsp = lowest_ordered_dsp;

    for (; NOT_END(param); ++param, ++arg) {
        //
        // Note: We don't want to immediately accept the ARG_MARKED_CHECKED as
        // hidden and done, because if the parameter wasn't hidden at the
        // outset it hasn't been typechecked yet.
        //
        // !!! Should PROTECT/HIDE do the type checking at the PROTECT if it
        // detects the field is in a FRAME!?

        if (Is_Param_Hidden(param, param))  // ^-- note why special = param
            continue;

        if (TYPE_CHECK(param, REB_TS_REFINEMENT)) {
            if (
                Is_Void_With_Sym(arg, SYM_UNSET)
                and NOT_CELL_FLAG(arg, ARG_MARKED_CHECKED)
            ){
                // Undefined refinements not explicitly marked hidden are
                // still candidates for usage at the callsite.

                goto unspecialized_arg;  // ran out...no pre-empt needed
            }

            if (GET_CELL_FLAG(arg, ARG_MARKED_CHECKED))
                assert(IS_NULLED(arg) or Is_Blackhole(arg));
            else
                Typecheck_Refinement(param, arg);

            goto specialized_arg_no_typecheck;
        }

        // It's an argument, either a normal one or a refinement arg.

        if (
            Is_Void_With_Sym(arg, SYM_UNSET)
            and NOT_CELL_FLAG(arg, ARG_MARKED_CHECKED)
        ){
            goto unspecialized_arg;
        }

        goto specialized_arg_with_check;

      unspecialized_arg:

        assert(NOT_CELL_FLAG(arg, ARG_MARKED_CHECKED));
        assert(Is_Void_With_Sym(arg, SYM_UNSET));
        continue;

      specialized_arg_with_check:

        // !!! If argument was previously specialized, should have been type
        // checked already... don't type check again (?)
        //
        if (Is_Param_Variadic(param))
            fail ("Cannot currently SPECIALIZE variadic arguments.");

        if (not TYPE_CHECK(param, VAL_TYPE(arg)))
            fail (arg);  // !!! merge w/Error_Invalid_Arg()

       SET_CELL_FLAG(arg, ARG_MARKED_CHECKED);

      specialized_arg_no_typecheck:

        // Specialized-out arguments must still be in the parameter list,
        // for enumeration in the evaluator to line up with the frame values
        // of the underlying function.

        assert(GET_CELL_FLAG(arg, ARG_MARKED_CHECKED));
        continue;
    }

    // Everything should have balanced out for a valid specialization.
    // Turn partial refinements into an array of things to push.
    //
    REBARR *partials;
    if (ordered_dsp == DSP)
        partials = nullptr;
    else {
        // The list of ordered refinements may contain some cases like /ONLY
        // which aren't considered partial because they have no argument.
        // If that's the only kind of partial we hvae, we'll free this array.
        //
        // !!! This array will be allocated too big in cases like /dup/only,
        // review how to pick the exact size efficiently.  There's also the
        // case that duplicate refinements or non-existent ones create waste,
        // but since we error and throw those arrays away it doesn't matter.
        //
        partials = Make_Array_Core(
            DSP - ordered_dsp,  // maximum partial count possible
            SERIES_MASK_PARTIALS  // don't manage, yet... may free
        );

        while (ordered_dsp != DSP) {
            ++ordered_dsp;
            STKVAL(*) ordered = DS_AT(ordered_dsp);
            if (not IS_WORD_BOUND(ordered))  // specialize :print/asdf
                fail (Error_Bad_Refine_Raw(ordered));

            REBVAL *slot = CTX_VAR(exemplar, VAL_WORD_INDEX(ordered));
            if (NOT_CELL_FLAG(slot, ARG_MARKED_CHECKED)) {
                assert(Is_Void_With_Sym(slot, SYM_UNSET));

                // It's still partial...
                //
                Init_Any_Word_Bound(
                    Alloc_Tail_Array(partials),
                    REB_SYM_WORD,
                    exemplar,
                    VAL_WORD_INDEX(ordered)
                );
            }
            assert(not IS_NULLED(slot));
        }
        DS_DROP_TO(lowest_ordered_dsp);

        if (ARR_LEN(partials) == 0) {
            Free_Unmanaged_Series(partials);
            partials = nullptr;
        }
        else {
            LINK_PARTIALS_VARLIST_OR_PARAMLIST_NODE(partials) = NOD(exemplar);
            Manage_Series(partials);
        }
    }

    REBACT *specialized = Make_Action(
        partials != nullptr ? partials : CTX_VARLIST(exemplar),
        nullptr,  // meta inherited by SPECIALIZE helper to SPECIALIZE*
        &Specializer_Dispatcher,
        IDX_SPECIALIZER_MAX  // details array capacity
    );
    assert(CTX_KEYLIST(exemplar) == ACT_PARAMLIST(unspecialized));


    Init_Action(out, specialized, VAL_ACTION_LABEL(specializee), UNBOUND);

    return false;  // code block did not throw
}


//
//  specialize*: native [
//
//  {Create a new action through partial or full specialization of another}
//
//      return: [action!]
//      specializee "Function whose parameters will be set to fixed values"
//          [action!]
//      def "Definition for FRAME! fields for args and refinements"
//          [block!]
//  ]
//
REBNATIVE(specialize_p)  // see extended definition SPECIALIZE in %base-defs.r
{
    INCLUDE_PARAMS_OF_SPECIALIZE_P;

    REBVAL *specializee = ARG(specializee);

    // Refinement specializations via path are pushed to the stack, giving
    // order information that can't be meaningfully gleaned from an arbitrary
    // code block (e.g. `specialize :append [dup: x | if y [part: z]]`, we
    // shouldn't think that intends any ordering of /dup/part or /part/dup)
    //
    REBDSP lowest_ordered_dsp = DSP; // capture before any refinements pushed

    // !!! When SPECIALIZE would take a PATH! instead of an action, this is
    // where refinements could be pushed to weave into the specialization.
    // To make the interface less confusing, we no longer do this...but we
    // could push refinements here if we wanted to.

    if (Specialize_Action_Throws(
        D_OUT,
        specializee,
        ARG(def),
        lowest_ordered_dsp
    )){
        return R_THROWN;  // e.g. `specialize :append/dup [value: throw 10]`
    }

    return D_OUT;
}


//
//  For_Each_Unspecialized_Param: C
//
// We have to take into account specialization of refinements in order to know
// the correct order.  If someone has:
//
//     foo: func [a [integer!] /b [integer!] /c [integer!]] [...]
//
// They can partially specialize this as :foo/c/b.  This makes it seem to the
// caller a function originally written with spec:
//
//     [a [integer!] c [integer!] b [integer!]]
//
// But the frame order doesn't change; the information for knowing the order
// is encoded in a "partials" array.  See remarks on ACT_PARTIALS().
//
// The true order could be cached when the function is generated, but to keep
// things "simple" we capture the behavior in this routine.
//
// Unspecialized parameters are visited in two passes: unsorted, then sorted.
//
void For_Each_Unspecialized_Param(
    REBACT *act,
    PARAM_HOOK hook,
    void *opaque
){
    option(REBARR*) partials = ACT_PARTIALS(act);

    // Walking the parameters in a potentially "unsorted" fashion.  Offer them
    // to the passed-in hook in case it has a use for this first pass (e.g.
    // just counting, to make an array big enough to hold what's going to be
    // given to it in the second pass.
    //
  blockscope {
    REBVAL *param = ACT_PARAMS_HEAD(act);
    REBVAL *special = ACT_SPECIALTY_HEAD(act);

    // Loop through and pass just the normal args.
    //
    for (; NOT_END(param); ++param, ++special) {
        if (Is_Param_Hidden(param, special))
            continue;

        if (TYPE_CHECK(param, REB_TS_REFINEMENT))
            continue;

        Reb_Param_Class pclass = VAL_PARAM_CLASS(param);
        if (pclass == REB_P_LOCAL)
            continue;

        // If the modal parameter has had its refinement specialized out, it
        // is no longer modal.
        //
        REBFLGS flags = 0;
        if (pclass == REB_P_MODAL) {
            if (NOT_END(param + 1)) {  // !!! Ideally checked at creation
                if (GET_CELL_FLAG(special + 1, ARG_MARKED_CHECKED)) {
                    if (TYPE_CHECK(param + 1, REB_TS_REFINEMENT))  // required
                        flags |= PHF_DEMODALIZED;  // !!! ^-- check at create!
                }
            }
        }

        bool cancel = not hook(param, flags, opaque);
        if (cancel)
            return;
    }
  }

    // Now jump around and take care of the partial refinements.

    if (partials) {
        assert(ARR_LEN(unwrap(partials)) > 0);  // no partials means no array
 
        // the highest priority are at *top* of stack, so we have to go
        // "downward" in the push order...e.g. the reverse of the array.

        REBVAL *partial = SPECIFIC(ARR_TAIL(unwrap(partials)));
        REBVAL *head = SPECIFIC(ARR_HEAD(unwrap(partials)));
        for (; partial-- != head; ) {
            REBVAL *param = ACT_PARAM(act, VAL_WORD_INDEX(partial));

            bool cancel = not hook(param, PHF_UNREFINED, opaque);
            if (cancel)
                return;
        }
    }

    // Finally, output any fully unspecialized refinements

  blockscope {
    REBVAL *param = ACT_PARAMS_HEAD(act);
    REBVAL *special = ACT_SPECIALTY_HEAD(act);

    for (; NOT_END(param); ++param, ++special) {
        if (Is_Param_Hidden(param, special))
            continue;

        if (not TYPE_CHECK(param, REB_TS_REFINEMENT))
            continue;

        if (partials) {
            REBVAL *partial = SPECIFIC(ARR_HEAD(unwrap(partials)));
            for (; NOT_END(partial); ++partial) {
                if (SAME_STR(
                    VAL_WORD_SPELLING(partial),
                    VAL_PARAM_SPELLING(param)
                )){
                    goto continue_unspecialized_loop;
                }
            }
        }

        bool cancel = not hook(param, 0, opaque);
        if (cancel)
            return;

      continue_unspecialized_loop:
        NOOP;
    }
  }
}


struct Find_Param_State {
    REBVAL *param;
};

static bool First_Param_Hook(REBVAL *param, REBFLGS flags, void *opaque)
{
    struct Find_Param_State *s = cast(struct Find_Param_State*, opaque);
    assert(not s->param);  // should stop enumerating if found

    if (not (flags & PHF_UNREFINED) and TYPE_CHECK(param, REB_TS_REFINEMENT))
        return false;  // we know WORD!-based invocations will be 0 arity

    s->param = param;
    return false;  // found first unspecialized, no need to look more
}

static bool Last_Param_Hook(REBVAL *param, REBFLGS flags, void *opaque)
{
    struct Find_Param_State *s = cast(struct Find_Param_State*, opaque);

    if (not (flags & PHF_UNREFINED) and TYPE_CHECK(param, REB_TS_REFINEMENT))
        return false;  // we know WORD!-based invocations will be 0 arity

    s->param = param;
    return true;  // keep looking and be left with the last
}

//
//  First_Unspecialized_Param: C
//
// This can be somewhat complex in the worst case:
//
//     >> foo: func [/a [block!] /b [block!] /c [block!] /d [block!]] [...]
//     >> foo-d: :foo/d
//
// This means that the last parameter (D) is actually the first of FOO-D.
//
REBVAL *First_Unspecialized_Param(REBACT *act)
{
    struct Find_Param_State s;
    s.param = nullptr;

    For_Each_Unspecialized_Param(act, &First_Param_Hook, &s);

    return s.param;  // may be nullptr
}


//
//  Last_Unspecialized_Param: C
//
// See notes on First_Unspecialized_Param() regarding complexity
//
REBVAL *Last_Unspecialized_Param(REBACT *act)
{
    struct Find_Param_State s;
    s.param = nullptr;

    For_Each_Unspecialized_Param(act, &Last_Param_Hook, &s);

    return s.param;  // may be nullptr
}

//
//  First_Unspecialized_Arg: C
//
// Helper built on First_Unspecialized_Param(), can also give you the param.
//
REBVAL *First_Unspecialized_Arg(option(REBVAL **) param_out, REBFRM *f)
{
    REBACT *phase = FRM_PHASE(f);
    REBVAL *param = First_Unspecialized_Param(phase);
    if (param_out)
        *unwrap(param_out) = param;

    if (param == nullptr)
        return nullptr;

    REBLEN index = param - ACT_PARAMS_HEAD(phase);
    return FRM_ARGS_HEAD(f) + index;
}


//
//  Make_Invocation_Frame_Throws: C
//
// Logic shared currently by DOES and MATCH to build a single executable
// frame from feeding forward a VARARGS! parameter.  A bit like being able to
// call EVALUATE via Eval_Core() yet introspect the evaluator step.
//
bool Make_Invocation_Frame_Throws(
    REBFRM *f,
    REBVAL **first_arg_ptr,  // returned so that MATCH can steal it
    const REBVAL *action
){
    assert(IS_ACTION(action));
    assert(f == FS_TOP);

    // It is desired that any nulls encountered be processed as if they are
    // not specialized...and gather at the callsite if necessary.
    //
    f->flags.bits |=
        EVAL_FLAG_ERROR_ON_DEFERRED_ENFIX;  // can't deal with ELSE/THEN/etc.

    // === END FIRST PART OF CODE FROM DO_SUBFRAME ===

    option(const REBSTR*) label = nullptr;  // !!! for now
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

    if (threw)
        return true;

    // === END SECOND PART OF CODE FROM DO_SUBFRAME ===

    *first_arg_ptr = nullptr;

    REBVAL *param = CTX_KEYS_HEAD(CTX(f->varlist));
    REBVAL *arg = CTX_VARS_HEAD(CTX(f->varlist));
    for (; NOT_END(param); ++param, ++arg) {
        Reb_Param_Class pclass = VAL_PARAM_CLASS(param);
        if (TYPE_CHECK(param, REB_TS_REFINEMENT))
            continue;  // optional so doesn't count

        switch (pclass) {
          case REB_P_NORMAL:
          case REB_P_HARD:
          case REB_P_MODAL:
          case REB_P_MEDIUM:
          case REB_P_SOFT:
            *first_arg_ptr = arg;
            goto found_first_arg_ptr;

          case REB_P_LOCAL:
          case REB_P_SEALED:
            break;

          case REB_P_OUTPUT:  // should always have REB_TS_REFINEMENT
          default:
            panic ("Unknown PARAM_CLASS");
        }
    }

    fail ("ACTION! has no args to MAKE FRAME! from...");

  found_first_arg_ptr:

    // DS_DROP_TO(lowest_ordered_dsp);

    return false;
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

    const REBSTR *label = VAL_ACTION_LABEL(out);

    DECLARE_LOCAL (action);
    Move_Value(action, out);
    PUSH_GC_GUARD(action);

    // We interpret phrasings like `x: does all [...]` to mean something
    // like `x: specialize :all [block: [...]]`.  While this originated
    // from the Rebmu code golfing language to eliminate a pair of bracket
    // characters from `x: does [all [...]]`, it actually has different
    // semantics...which can be useful in their own right, plus the
    // resulting function will run faster.

    REBVAL *first_arg;
    if (Make_Invocation_Frame_Throws(f, &first_arg, action)) {
        DROP_GC_GUARD(action);
        return true;
    }

    UNUSED(first_arg); // MATCH uses to get its answer faster, we don't need

    REBACT *act = VAL_ACTION(action);

    assert(NOT_SERIES_FLAG(f->varlist, MANAGED)); // not invoked yet
    assert(FRM_BINDING(f) == VAL_ACTION_BINDING(action));

    REBCTX *exemplar = Steal_Context_Vars(
        CTX(f->varlist),
        NOD(ACT_PARAMLIST(act))
    );
    assert(ACT_NUM_PARAMS(act) == CTX_LEN(exemplar));

    INIT_LINK_KEYSOURCE(CTX_VARLIST(exemplar), NOD(ACT_PARAMLIST(act)));

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


//
//  Alloc_Action_From_Exemplar: C
//
// Leaves details blank, and lets you specify the dispatcher.
//
REBACT *Alloc_Action_From_Exemplar(
    REBCTX *exemplar,
    REBNAT dispatcher,
    REBLEN details_capacity
){
    REBACT *unspecialized = CTX_FRAME_ACTION(exemplar);

    REBVAL *param = ACT_PARAMS_HEAD(unspecialized);
    REBVAL *arg = CTX_VARS_HEAD(exemplar);
    for (; NOT_END(param); ++param, ++arg) {
        if (GET_CELL_FLAG(arg, ARG_MARKED_CHECKED))
            continue;

        assert(not Is_Param_Hidden(param, param));  // param = special

        // We leave non-hidden ~unset~ as-is to be handled by the evaluator
        // as unspecialized:
        //
        // https://forum.rebol.info/t/default-values-and-make-frame/1412
        // https://forum.rebol.info/t/1413
        //
        if (Is_Void_With_Sym(arg, SYM_UNSET))
            continue;

        if (TYPE_CHECK(param, REB_TS_REFINEMENT))
            Typecheck_Refinement(param, arg);
        else
            Typecheck_Including_Constraints(param, arg);

        SET_CELL_FLAG(arg, ARG_MARKED_CHECKED);
    }

    // This code parallels Specialize_Action_Throws(), see comments there

    REBACT *action = Make_Action(
        CTX_VARLIST(exemplar),  // note: no partials
        nullptr,  // no meta, REDESCRIBE can add help
        dispatcher,
        details_capacity
    );

    return action;
}


//
//  Make_Action_From_Exemplar: C
//
// Assumes you want a Specializer_Dispatcher with the exemplar in details.
//
REBACT *Make_Action_From_Exemplar(REBCTX *exemplar)
{
    REBACT *action = Alloc_Action_From_Exemplar(
        exemplar,
        &Specializer_Dispatcher,
        IDX_SPECIALIZER_MAX  // details capacity
    );
    return action;
}


//
//  partialize: native [
//
//  {Test new concept for partial refinements and parameter reordering}
//
//      return: [action!]
//      action [action!]
//      partials [block!]
//  ]
//
REBNATIVE(partialize)
{
    INCLUDE_PARAMS_OF_PARTIALIZE;

    REBVAL *copy = rebValue("copy", rebQ(ARG(action)), rebEND);
    REBACT *reordered = VAL_ACTION(copy);
    rebRelease(copy);

    REBCTX *exemplar = ACT_EXEMPLAR(reordered);
    if (not exemplar)
        fail ("PARTIALIZE experiment requires exemplar at the moment");

    REBDSP dsp_orig = DSP;

    REBARR *specialty = ACT_SPECIALTY(reordered);
    if (GET_ARRAY_FLAG(specialty, IS_PARTIALS)) {
        const REBVAL *word = SPECIFIC(ARR_HEAD(specialty));
        for (; NOT_END(word); ++word) {
            assert(IS_WORD_BOUND(word));
            Move_Value(DS_PUSH(), word);
        }
        specialty = ARR(LINK_PARTIALS_VARLIST_OR_PARAMLIST_NODE(specialty));
    }

    // We need to bind the incoming words to what's visible, as hidden words
    // may exist in internal compositions.
    //
    const RELVAL *item = VAL_ARRAY_AT(ARG(partials));
    for (; NOT_END(item); ++item) {
        REBVAL *param = ACT_PARAMS_HEAD(reordered);
        REBVAL *special = ACT_SPECIALTY_HEAD(reordered);
        REBLEN index = 1;
        const REBSTR *spelling = VAL_WORD_SPELLING(item);
        for (; NOT_END(param); ++param, ++special, ++index) {
            if (Is_Param_Hidden(param, special))
                continue;

            if (VAL_PARAM_SPELLING(param) == spelling) {
                Init_Any_Word_Bound(DS_PUSH(), REB_SYM_WORD, exemplar, index);
                goto next_item;
            }
        }

        fail (rebUnrelativize(item));

      next_item: {}
    }

    REBARR *partials = Pop_Stack_Values_Core(
        dsp_orig,
        SERIES_FLAG_MANAGED | SERIES_MASK_PARTIALS
    );
    LINK_PARTIALS_VARLIST_OR_PARAMLIST_NODE(partials) = NOD(specialty);
    REBVAL *archetype = ACT_ARCHETYPE(reordered);
    VAL_ACTION_SPECIALTY_OR_LABEL_NODE(archetype) = NOD(partials);

    return Init_Action(
        D_OUT,
        reordered,
        VAL_ACTION_LABEL(ARG(action)),
        VAL_ACTION_BINDING(ARG(action))
    );
}
