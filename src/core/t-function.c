//
//  File: %t-function.c
//  Summary: "function related datatypes"
//  Section: datatypes
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

#include "sys-core.h"

static bool Same_Action(REBCEL(const*) a, REBCEL(const*) b)
{
    assert(CELL_KIND(a) == REB_ACTION and CELL_KIND(b) == REB_ACTION);

    if (VAL_ACTION_KEYLIST(a) == VAL_ACTION_KEYLIST(b)) {
        //
        // All actions that have the same paramlist are not necessarily the
        // "same action".  For instance, every RETURN shares a common
        // paramlist, but the binding is different in the REBVAL instances
        // in order to know where to "exit from".
        //
        return VAL_ACTION_BINDING(a) == VAL_ACTION_BINDING(b);
    }

    return false;
}


//
//  CT_Action: C
//
REBINT CT_Action(REBCEL(const*) a, REBCEL(const*) b, bool strict)
{
    UNUSED(strict);  // no lax form of comparison

    if (Same_Action(a, b))
        return 0;
    assert(VAL_ACTION(a) != VAL_ACTION(b));
    return a > b ? 1 : -1;  // !!! Review arbitrary ordering
}


//
//  MAKE_Action: C
//
// Ren-C provides the ability to MAKE ACTION! from a FRAME!.  Any values on
// the public interface which are ~unset~ will be assumed to be unspecialized.
//
// https://forum.rebol.info/t/default-values-and-make-frame/1412
//
// It however does not carry forward R3-Alpha's concept of MAKE ACTION! from
// a BLOCK!, e.g. `make function! copy/deep reduce [spec body]`.  This is
// because there is no particular advantage to folding the two parameters to
// FUNC into one block...and it makes spec analysis seem more "cooked in"
// than being an epicycle of the design of FUNC (which is just an optimized
// version of something that could be written in usermode).
//
REB_R MAKE_Action(
    REBVAL *out,
    enum Reb_Kind kind,
    option(const REBVAL*) parent,
    const REBVAL *arg
){
    assert(kind == REB_ACTION);
    if (parent)
        fail (Error_Bad_Make_Parent(kind, unwrap(parent)));

    if (IS_FRAME(arg)) {  // will assume ~unset~ fields are unspecialized
        //
        // !!! This makes a copy of the incoming context.  AS FRAME! does not,
        // but it expects any specialized frame fields to be hidden, and non
        // hidden fields are parameter specifications.  Review if there is
        // some middle ground.
        //
        REBVAL *frame_copy = rebValue("copy", arg);
        REBCTX *exemplar = VAL_CONTEXT(frame_copy);
        rebRelease(frame_copy);

        return Init_Action(
            out,
            Make_Action_From_Exemplar(exemplar),
            VAL_FRAME_LABEL(arg),
            VAL_FRAME_BINDING(arg)
        );
    }

    if (not IS_BLOCK(arg))
        fail (Error_Bad_Make(REB_ACTION, arg));

    fail ("Ren-C does not support MAKE ACTION! on BLOCK! (see FUNC*/FUNC)");
}


//
//  TO_Action: C
//
// There is currently no meaning for TO ACTION!.  DOES will create an action
// from a BLOCK!, e.g. `x: does [1 + y]`, so TO ACTION! of a block doesn't
// need to do that (for instance).
//
REB_R TO_Action(REBVAL *out, enum Reb_Kind kind, const REBVAL *arg)
{
    assert(kind == REB_ACTION);
    UNUSED(kind);

    UNUSED(out);

    fail (arg);
}


//
//  MF_Action: C
//
void MF_Action(REB_MOLD *mo, REBCEL(const*) v, bool form)
{
    UNUSED(form);

    Append_Ascii(mo->series, "#[action! ");

    option(const REBSTR*) label = VAL_ACTION_LABEL(v);
    if (label) {
        Append_Codepoint(mo->series, '{');
        Append_Spelling(mo->series, unwrap(label));
        Append_Ascii(mo->series, "} ");
    }

    // !!! The system is no longer keeping the spec of functions, in order
    // to focus on a generalized "meta info object" service.  MOLD of
    // functions temporarily uses the word list as a substitute (which
    // drops types)
    //
    const bool just_words = false;
    REBARR *parameters = Make_Action_Parameters_Arr(VAL_ACTION(v), just_words);
    Mold_Array_At(mo, parameters, 0, "[]");
    Free_Unmanaged_Series(parameters);

    // !!! Previously, ACTION! would mold the body out.  This created a large
    // amount of output, and also many function variations do not have
    // ordinary "bodies".  It's more useful to show the cached name, and maybe
    // some base64 encoding of a UUID (?)  In the meantime, having the label
    // of the last word used is actually a lot more useful than most things.

    Append_Codepoint(mo->series, ']');
    End_Mold(mo);
}


//
//  REBTYPE: C
//
REBTYPE(Action)
{
    REBVAL *action = D_ARG(1);
    REBACT *act = VAL_ACTION(action);

    switch (VAL_WORD_ID(verb)) {
      case SYM_COPY: {
        INCLUDE_PARAMS_OF_COPY;

        UNUSED(PAR(value));

        if (REF(part) or REF(types))
            fail (Error_Bad_Refines_Raw());

        if (REF(deep)) {
            // !!! always "deep", allow it?
        }

        // Copying functions creates another handle which executes the same
        // code, yet has a distinct identity.  This means it would not be
        // HIJACK'd if the function that it was copied from was hijacked.

        REBCTX *meta = ACT_META(act);  // !!! Note: not a copy of meta

        // If the function had code, then that code will be bound relative
        // to the original paramlist that's getting hijacked.  So when the
        // proxy is called, we want the frame pushed to be relative to
        // whatever underlied the function...even if it was foundational
        // so `underlying = VAL_ACTION(value)`

        REBLEN details_len = ARR_LEN(ACT_DETAILS(act));
        REBACT *proxy = Make_Action(
            ACT_SPECIALTY(act),  // not changing the interface
            ACT_DISPATCHER(act),
            details_len  // details array capacity
        );

        assert(ACT_META(proxy) == nullptr);
        mutable_ACT_META(proxy) = meta;

        if (GET_ACTION_FLAG(act, IS_NATIVE))
            SET_ACTION_FLAG(proxy, IS_NATIVE);

        // A new body_holder was created inside Make_Action().  Rare case
        // where we can bit-copy a possibly-relative value.
        //
      blockscope {
        const RELVAL *src_tail = ARR_TAIL(ACT_DETAILS(act));
        RELVAL *src = ARR_HEAD(ACT_DETAILS(act)) + 1;
        RELVAL *dest = ARR_HEAD(ACT_DETAILS(proxy)) + 1;
        for (; src != src_tail; ++src, ++dest)
            Copy_Cell(dest, src);
        SET_SERIES_LEN(ACT_DETAILS(proxy), details_len);
      }

        return Init_Action(
            D_OUT,
            proxy,
            VAL_ACTION_LABEL(action),  // keep symbol (if any) from original
            VAL_ACTION_BINDING(action)  // same (e.g. RETURN to same frame)
        ); }

      case SYM_REFLECT: {
        INCLUDE_PARAMS_OF_REFLECT;
        UNUSED(ARG(value));

        REBVAL *property = ARG(property);
        SYMID sym = VAL_WORD_ID(property);
        switch (sym) {
          case SYM_BINDING: {
            if (Did_Get_Binding_Of(D_OUT, action))
                return D_OUT;
            return nullptr; }

          case SYM_LABEL: {
            option(const REBSYM*) label = VAL_ACTION_LABEL(action);
            if (not label)
                return nullptr;
            return Init_Word(D_OUT, unwrap(label)); }

          case SYM_WORDS:
          case SYM_PARAMETERS: {
            bool just_words = (sym == SYM_WORDS);
            return Init_Block(
                D_OUT,
                Make_Action_Parameters_Arr(act, just_words)
            ); }

          case SYM_BODY:
            Get_Maybe_Fake_Action_Body(D_OUT, action);
            return D_OUT;

          case SYM_TYPES:
            return Copy_Cell(D_OUT, CTX_ARCHETYPE(ACT_EXEMPLAR(act)));

          case SYM_FILE:
          case SYM_LINE: {
            //
            // Use a heuristic that if the first element of a function's body
            // is a series with the file and line bits set, then that's what
            // it returns for FILE OF and LINE OF.

            REBARR *details = ACT_DETAILS(act);
            if (ARR_LEN(details) < 1 or not ANY_ARRAY(ARR_HEAD(details)))
                return nullptr;

            const REBARR *a = VAL_ARRAY(ARR_HEAD(details));
            if (NOT_SUBCLASS_FLAG(ARRAY, a, HAS_FILE_LINE_UNMASKED))
                return nullptr;

            // !!! How to tell URL! vs FILE! ?
            //
            if (VAL_WORD_ID(property) == SYM_FILE)
                Init_File(D_OUT, LINK(Filename, a));
            else
                Init_Integer(D_OUT, a->misc.line);

            return D_OUT; }

          default:
            fail (Error_Cannot_Reflect(REB_ACTION, property));
        }
        break; }

      default:
        break;
    }

    return R_UNHANDLED;
}


//
//  PD_Action: C
//
// We *could* generate a partially specialized action variant at each step:
//
//     `append/dup/only` => `ad: :append/dup | ado: :ad/only | ado`
//
// But generating these intermediates would be quite costly.  So what is done
// instead is each step pushes a canonized word to the stack.  The processing
// for GET-PATH! will--at the end--make a partially refined ACTION! value
// (see WORD_FLAG_PARTIAL_REFINE).  But the processing for REB_PATH in
// Eval_Core() does not need to...it operates off stack values directly.
//
REB_R PD_Action(
    REBPVS *pvs,
    const RELVAL *picker,
    option(const REBVAL*) setval
){
    UNUSED(setval);

    assert(IS_ACTION(pvs->out));

    if (IS_NULLED_OR_BLANK(picker)) {  // !!! BLANK! used in bootstrap scripts
        //
        // Leave the function value as-is, and continue processing.  This
        // enables things like `append/(if only [/only])/dup`...
        //
        // Note this feature doesn't have obvious applications to refinements
        // that take arguments...only ones that don't.  If a refinement takes
        // an argument then you should supply it normally and then use NULL
        // in that argument slot to "revoke" it (the call will appear as if
        // the refinement was never used at the callsite).
        //
        return pvs->out;
    }

    // The first evaluation of a GROUP! and GET-WORD! are processed by the
    // general path mechanic before reaching this dispatch.  So if it's not
    // a word/refinement or or one of those that evaluated it, then error.
    //
    const REBSYM *symbol;
    if (IS_WORD(picker))
        symbol = VAL_WORD_SYMBOL(picker);
    else if (IS_PATH(picker) and IS_REFINEMENT(picker))
        symbol = VAL_REFINEMENT_SYMBOL(picker);
    else
        return R_UNHANDLED;

    Init_Word(DS_PUSH(), symbol);

    return pvs->out; // leave ACTION! value in pvs->out, as-is
}
