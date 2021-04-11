//
//  File: %t-object.c
//  Summary: "object datatype"
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


static void Append_To_Context(REBVAL *context, REBVAL *arg)
{
    REBCTX *c = VAL_CONTEXT(context);

    if (ANY_WORD(arg)) {  // Add an unset word: `append context 'some-word`
        const bool strict = true;
        if (0 == Find_Symbol_In_Context(
            context,
            VAL_WORD_SYMBOL(arg),
            strict
        )){
            Expand_Context(c, 1);
            Append_Context(c, nullptr, VAL_WORD_SYMBOL(arg));
        }
        return;
    }

    if (not IS_BLOCK(arg))
        fail (arg);

    const RELVAL *tail;
    const RELVAL *item = VAL_ARRAY_AT(&tail, arg);

    // Can't actually fail() during a collect, so make sure any errors are
    // set and then jump to a Collect_End()
    //
    REBCTX *error = nullptr;

    struct Reb_Collector collector;
    Collect_Start(&collector, COLLECT_ANY_WORD);

  blockscope {  // Start out binding table with words already in context
    const REBSTR *duplicate;
    Collect_Context_Keys(&duplicate, &collector, c);
    assert(not duplicate);  // context should have all unique keys
  }

    REBLEN first_new_index = Collector_Index_If_Pushed(&collector);

    // Do a pass to collect the [set-word: <value>] keys and add them to the
    // binder.  But don't modify the object yet, in case the block turns out
    // to be malformed (we don't want partial expansions applied).
    //
    // !!! This allows plain WORD! in the key spot, in addition to SET-WORD!.
    // Should it allow ANY-WORD!?  Restrict to just SET-WORD!?
    //
  blockscope {
    const RELVAL *word;
    for (word = item; word != tail; word += 2) {
        if (not IS_WORD(word) and not IS_SET_WORD(word)) {
            error = Error_Bad_Value_Core(word, VAL_SPECIFIER(arg));
            goto collect_end;
        }

        const REBSYM *symbol = VAL_WORD_SYMBOL(word);

        if (Try_Add_Binder_Index(
            &collector.binder,
            symbol,
            Collector_Index_If_Pushed(&collector)
        )){
            Init_Word(DS_PUSH(), VAL_WORD_SYMBOL(word));
        }
        if (word + 1 == tail)  // catch malformed case with no value (#708)
            break;
    }
  }

  blockscope {  // Append new words to obj
    REBLEN num_added = Collector_Index_If_Pushed(&collector) - first_new_index;
    Expand_Context(c, num_added);

    STKVAL(*) new_word = DS_AT(collector.dsp_orig) + first_new_index;
    for (; new_word != DS_TOP + 1; ++new_word)
        Append_Context(c, nullptr, VAL_WORD_SYMBOL(new_word));
  }

  blockscope {  // Set new values to obj words
    const RELVAL *word = item;
    for (; word != tail; word += 2) {
        REBLEN i = Get_Binder_Index_Else_0(
            &collector.binder, VAL_WORD_SYMBOL(word)
        );
        assert(i != 0);

        const REBKEY *key = CTX_KEY(c, i);
        REBVAR *var = CTX_VAR(c, i);

        if (GET_CELL_FLAG(var, PROTECTED)) {
            error = Error_Protected_Key(key);
            goto collect_end;
        }

        if (Is_Var_Hidden(var)) {
            error = Error_Hidden_Raw();
            goto collect_end;
        }

        if (word + 1 == tail) {
            Init_Void(var, SYM_VOID);
            break;  // fix bug#708
        }
        else
            Derelativize(var, &word[1], VAL_SPECIFIER(arg));
    }
  }

  collect_end:
    Collect_End(&collector);

    if (error)
        fail (error);
}


//
//  CT_Context: C
//
REBINT CT_Context(REBCEL(const*) a, REBCEL(const*) b, bool strict)
{
    assert(ANY_CONTEXT_KIND(CELL_KIND(a)));
    assert(ANY_CONTEXT_KIND(CELL_KIND(b)));

    if (CELL_KIND(a) != CELL_KIND(b))  // e.g. ERROR! won't equal OBJECT!
        return CELL_KIND(a) > CELL_KIND(b) ? 1 : 0;

    REBCTX *c1 = VAL_CONTEXT(a);
    REBCTX *c2 = VAL_CONTEXT(b);
    if (c1 == c2)
        return 0;  // short-circuit, always equal if same context pointer

    // Note: can't short circuit on unequal frame lengths alone, as hidden
    // fields of objects do not figure into the `equal?` of their public
    // portions.

    const REBKEY *tail1;
    const REBKEY *key1 = CTX_KEYS(&tail1, c1);
    const REBKEY *tail2;
    const REBKEY *key2 = CTX_KEYS(&tail2, c2);
    const REBVAR *var1 = CTX_VARS_HEAD(c1);
    const REBVAR *var2 = CTX_VARS_HEAD(c2);

    // Compare each entry, in order.  Skip any hidden fields, field names are
    // compared case-insensitively.
    //
    // !!! The order dependence suggests that `make object! [a: 1 b: 2]` will
    // not be equal to `make object! [b: 1 a: 2]`.  See #2341
    //
    for (
        ;
        key1 != tail1 and key2 != tail2;
        ++key1, ++key2, ++var1, ++var2
    ){
      no_advance:
        if (Is_Var_Hidden(var1)) {
            ++key1;
            ++var1;
            if (key1 == tail1)
                break;
            goto no_advance;
        }
        if (Is_Var_Hidden(var2)) {
            ++key2;
            ++var2;
            if (key2 == tail2)
                break;
            goto no_advance;
        }

        const REBSYM *symbol1 = KEY_SYMBOL(key1);
        const REBSYM *symbol2 = KEY_SYMBOL(key2);
        REBINT spell_diff = Compare_Spellings(symbol1, symbol2, strict);
        if (spell_diff != 0)
            return spell_diff;

        REBINT diff = Cmp_Value(var1, var2, strict);
        if (diff != 0)
            return diff;
    }

    // Either key1 or key2 is at the end here, but the other might contain
    // all hidden values.  Which is okay.  But if a value isn't hidden,
    // they don't line up.
    //
    for (; key1 != tail1; key1++, var1++) {
        if (not Is_Var_Hidden(var1))
            return 1;
    }
    for (; key2 != tail2; key2++, var2++) {
        if (not Is_Var_Hidden(var2))
            return -1;
    }

    return 0;
}


//
//  MAKE_Frame: C
//
// !!! The feature of MAKE FRAME! from a VARARGS! would be interesting as a
// way to support usermode authoring of things like MATCH.
//
// For now just support ACTION! (or path/word to specify an action)
//
REB_R MAKE_Frame(
    REBVAL *out,
    enum Reb_Kind kind,
    option(const REBVAL*) parent,
    const REBVAL *arg
){
    if (parent)
        fail (Error_Bad_Make_Parent(kind, unwrap(parent)));

    // MAKE FRAME! on a VARARGS! was an experiment designed before REFRAMER
    // existed, to allow writing things like REQUOTE.  It's still experimental
    // but has had its functionality unified with reframer, so that it doesn't
    // really cost that much to keep around.  Use it sparingly (if at all).
    //
    if (IS_VARARGS(arg)) {
        REBFRM *f_varargs;
        if (not Is_Frame_Style_Varargs_May_Fail(&f_varargs, arg))
            fail (
                "Currently MAKE FRAME! on a VARARGS! only works with a varargs"
                " which is tied to an existing, running frame--not one that is"
                " being simulated from a BLOCK! (e.g. MAKE VARARGS! [...])"
            );

        assert(Is_Action_Frame(f_varargs));

        if (Make_Frame_From_Feed_Throws(out, f_varargs->feed))
            return R_THROWN;

        return out;
    }

    REBDSP lowest_ordered_dsp = DSP;  // Data stack gathers any refinements

    if (not IS_ACTION(arg))
        fail (Error_Bad_Make(kind, arg));

    REBCTX *exemplar = Make_Context_For_Action(
        arg, // being used here as input (e.g. the ACTION!)
        lowest_ordered_dsp, // will weave in any refinements pushed
        nullptr // no binder needed, not running any code
    );

    // See notes in %c-specialize.c about the special encoding used to
    // put /REFINEMENTs in refinement slots (instead of true/false/null)
    // to preserve the order of execution.

    return Init_Frame(out, exemplar, VAL_ACTION_LABEL(arg));
}


//
//  TO_Frame: C
//
// Currently can't convert anything TO a frame; nothing has enough information
// to have an equivalent representation (an OBJECT! could be an expired frame
// perhaps, but still would have no ACTION OF property)
//
REB_R TO_Frame(REBVAL *out, enum Reb_Kind kind, const REBVAL *arg)
{
    UNUSED(out);
    fail (Error_Bad_Make(kind, arg));
}


//
//  MAKE_Context: C
//
REB_R MAKE_Context(
    REBVAL *out,
    enum Reb_Kind kind,
    option(const REBVAL*) parent,
    const REBVAL *arg
){
    // Other context kinds (FRAME!, ERROR!, PORT!) have their own hooks.
    //
    assert(kind == REB_OBJECT or kind == REB_MODULE);

    option(REBCTX*) parent_ctx = parent
        ? VAL_CONTEXT(unwrap(parent))
        : nullptr;

    if (IS_BLOCK(arg)) {
        const RELVAL *tail;
        const RELVAL *at = VAL_ARRAY_AT(&tail, arg);

        REBCTX *ctx = Make_Context_Detect_Managed(
            REB_OBJECT,
            at,
            tail,
            parent_ctx
        );
        Init_Any_Context(out, kind, ctx); // GC guards it

        DECLARE_LOCAL (virtual_arg);
        Copy_Cell(virtual_arg, arg);

        Virtual_Bind_Deep_To_Existing_Context(
            virtual_arg,
            ctx,
            nullptr,  // !!! no binder made at present
            REB_WORD  // all internal refs are to the object
        );

        DECLARE_LOCAL (dummy);
        if (Do_Any_Array_At_Throws(dummy, virtual_arg, SPECIFIED)) {
            Move_Cell(out, dummy);
            return R_THROWN;
        }

        return out;
    }

    // `make object! 10` - currently not prohibited for any context type
    //
    if (ANY_NUMBER(arg)) {
        REBCTX *context = Make_Context_Detect_Managed(
            kind,
            END_CELL,  // values to scan for toplevel set-words (empty)
            END_CELL,
            parent_ctx
        );

        return Init_Any_Context(out, kind, context);
    }

    if (parent)
        fail (Error_Bad_Make_Parent(kind, unwrap(parent)));

    // make object! map!
    if (IS_MAP(arg)) {
        REBCTX *c = Alloc_Context_From_Map(VAL_MAP(arg));
        return Init_Any_Context(out, kind, c);
    }

    fail (Error_Bad_Make(kind, arg));
}


//
//  TO_Context: C
//
REB_R TO_Context(REBVAL *out, enum Reb_Kind kind, const REBVAL *arg)
{
    // Other context kinds (FRAME!, ERROR!, PORT!) have their own hooks.
    //
    assert(kind == REB_OBJECT or kind == REB_MODULE);

    if (kind == REB_OBJECT) {
        //
        // !!! Contexts hold canon values now that are typed, this init
        // will assert--a TO conversion would thus need to copy the varlist
        //
        return Init_Object(out, VAL_CONTEXT(arg));
    }

    fail (Error_Bad_Make(kind, arg));
}


//
//  PD_Context: C
//
REB_R PD_Context(
    REBPVS *pvs,
    const RELVAL *picker,
    option(const REBVAL*) setval
){
    REBCTX *c = VAL_CONTEXT(pvs->out);

    if (not IS_WORD(picker))
        return R_UNHANDLED;

    // See if the binding of the word is already to the context (so there's
    // no need to go hunting).  'x
    //
    REBLEN n;
    if (BINDING(picker) == c)
        n = VAL_WORD_INDEX(picker);
    else {
        const bool strict = false;
        n = Find_Symbol_In_Context(pvs->out, VAL_WORD_SYMBOL(picker), strict);

        if (n == 0)
            return R_UNHANDLED;

        // !!! As an experiment, try caching the binding index in the word.
        // This "corrupts" it, but if we say paths effectively own their
        // top-level words that could be all right.  Note this won't help if
        // the word is an evaluative product, as the bits live in the cell
        // and it will be discarded.
        //
        INIT_VAL_WORD_BINDING(m_cast(RELVAL*, picker), c);
        INIT_VAL_WORD_PRIMARY_INDEX(m_cast(RELVAL*, picker), n);
    }

    REBVAL *var = CTX_VAR(c, n);
    if (setval) {
        ENSURE_MUTABLE(pvs->out);

        if (GET_CELL_FLAG(var, PROTECTED))
            fail (Error_Protected_Word_Raw(rebUnrelativize(picker)));
    }

    pvs->u.ref.cell = var;
    pvs->u.ref.specifier = SPECIFIED;
    return R_REFERENCE;
}


//
//  meta-of: native [
//
//  {Get a reference to the "meta" context associated with a value.}
//
//      return: [<opt> any-context!]
//      value [<blank> action! any-context!]
//  ]
//
REBNATIVE(meta_of)  // see notes on MISC_META()
{
    INCLUDE_PARAMS_OF_META_OF;

    REBVAL *v = ARG(value);

    REBCTX *meta;
    if (IS_ACTION(v))
        meta = ACT_META(VAL_ACTION(v));
    else {
        assert(ANY_CONTEXT(v));
        meta = CTX_META(VAL_CONTEXT(v));
    }

    if (not meta)
        return nullptr;

    RETURN (CTX_ARCHETYPE(meta));
}


//
//  set-meta: native [
//
//  {Set "meta" object associated with all references to a value.}
//
//      return: [<opt> any-context!]
//      value [action! any-context!]
//      meta [<opt> any-context!]
//  ]
//
REBNATIVE(set_meta)
//
// See notes accompanying the `meta` field in the REBSER definition.
{
    INCLUDE_PARAMS_OF_SET_META;

    REBVAL *meta = ARG(meta);

    REBCTX *meta_ctx;
    if (ANY_CONTEXT(meta)) {
        if (IS_FRAME(meta))
            fail ("SET-META can't store context bindings, frames disallowed");

        meta_ctx = VAL_CONTEXT(meta);
    }
    else {
        assert(IS_NULLED(meta));
        meta_ctx = nullptr;
    }

    REBVAL *v = ARG(value);

    if (IS_ACTION(v))
        mutable_MISC(DetailsMeta, ACT_DETAILS(VAL_ACTION(v))) = meta_ctx;
    else
        mutable_MISC(VarlistMeta, CTX_VARLIST(VAL_CONTEXT(v))) = meta_ctx;

    RETURN (meta);
}


//
//  Copy_Context_Extra_Managed: C
//
// If no extra space is requested, the same keylist will be reused.
//
// !!! Copying a context used to be more different from copying an ordinary
// array.  But at the moment, much of the difference is that the marked bit
// in cells gets duplicated (so new context has the same VAR_MARKED_HIDDEN
// settings on its variables).  Review if the copying can be cohered better.
//
REBCTX *Copy_Context_Extra_Managed(
    REBCTX *original,
    REBLEN extra,
    REBU64 types
){
    ASSERT_SERIES_MANAGED(CTX_KEYLIST(original));
    assert(NOT_SERIES_FLAG(CTX_VARLIST(original), INACCESSIBLE));

    REBARR *varlist = Make_Array_For_Copy(
        CTX_LEN(original) + extra + 1,
        SERIES_MASK_VARLIST | NODE_FLAG_MANAGED,
        nullptr // original_array, N/A because LINK()/MISC() used otherwise
    );
    RELVAL *dest = ARR_HEAD(varlist);

    // The type information and fields in the rootvar (at head of the varlist)
    // get filled in with a copy, but the varlist needs to be updated in the
    // copied rootvar to the one just created.
    //
    Copy_Cell(dest, CTX_ARCHETYPE(original));
    INIT_VAL_CONTEXT_VARLIST(dest, varlist);

    ++dest;

    // Now copy the actual vars in the context, from wherever they may be
    // (might be in an array, or might be in the chunk stack for FRAME!)
    //
    const REBVAR *src_tail;
    REBVAL *src = CTX_VARS(&src_tail, original);
    for (; src != src_tail; ++src, ++dest) {
        Copy_Cell_Core(  // trying to duplicate slot precisely
            dest,
            src,
            CELL_MASK_COPY | CELL_FLAG_VAR_MARKED_HIDDEN
        );

        REBFLGS flags = NODE_FLAG_MANAGED;  // !!! Review, which flags?
        Clonify(dest, flags, types);
    }

    SET_SERIES_LEN(varlist, CTX_LEN(original) + 1);
    varlist->leader.bits |= SERIES_MASK_VARLIST;

    REBCTX *copy = CTX(varlist); // now a well-formed context

    if (extra == 0)
        INIT_CTX_KEYLIST_SHARED(copy, CTX_KEYLIST(original));  // ->link field
    else {
        assert(CTX_TYPE(original) != REB_FRAME);  // can't expand FRAME!s

        REBSER *keylist = Copy_Series_At_Len_Extra(
            CTX_KEYLIST(original),
            0,
            CTX_LEN(original),
            extra,
            SERIES_MASK_KEYLIST | NODE_FLAG_MANAGED
        );

        mutable_LINK(Ancestor, keylist) = CTX_KEYLIST(original);

        INIT_CTX_KEYLIST_UNIQUE(copy, keylist);  // ->link field
    }

    // A FRAME! in particular needs to know if it points back to a stack
    // frame.  The pointer is NULLed out when the stack level completes.
    // If we're copying a frame here, we know it's not running.
    //
    if (CTX_TYPE(original) == REB_FRAME)
        mutable_MISC(VarlistMeta, varlist) = nullptr;
    else {
        // !!! Should the meta object be copied for other context types?
        // Deep copy?  Shallow copy?  Just a reference to the same object?
        //
        mutable_MISC(VarlistMeta, varlist) = nullptr;
    }

    mutable_BONUS(Patches, varlist) = nullptr;  // no virtual bind patches yet

    return copy;
}


//
//  MF_Context: C
//
void MF_Context(REB_MOLD *mo, REBCEL(const*) v, bool form)
{
    REBSTR *s = mo->series;

    REBCTX *c = VAL_CONTEXT(v);

    // Prevent endless mold loop:
    //
    if (Find_Pointer_In_Series(TG_Mold_Stack, c) != NOT_FOUND) {
        if (not form) {
            Pre_Mold(mo, v); // If molding, get #[object! etc.
            Append_Codepoint(s, '[');
        }
        Append_Ascii(s, "...");

        if (not form) {
            Append_Codepoint(s, ']');
            End_Mold(mo);
        }
        return;
    }
    Push_Pointer_To_Series(TG_Mold_Stack, c);

    // Simple rule for starters: don't honor the hidden status of parameters
    // if the frame phase is executing.
    //
    bool honor_hidden = true;
    if (CELL_KIND(v) == REB_FRAME)
        honor_hidden = not IS_FRAME_PHASED(v);

    if (form) {
        //
        // Mold all words and their values ("key: <molded value>")
        //
        const REBKEY *tail;
        const REBKEY *key = CTX_KEYS(&tail, c);
        REBVAR *var = CTX_VARS_HEAD(c);
        REBPAR *param = (CELL_KIND(v) == REB_FRAME)
            ? ACT_PARAMS_HEAD(VAL_FRAME_PHASE(v))
            : cast_PAR(var);

        bool had_output = false;
        for (; key != tail; ++key, ++var, ++param) {
            if (honor_hidden and Is_Param_Hidden(param))
                continue;

            Append_Spelling(mo->series, KEY_SYMBOL(key));
            Append_Ascii(mo->series, ": ");
            Mold_Value(mo, var);
            Append_Codepoint(mo->series, LF);
            had_output = true;
        }

        // Remove the final newline...but only if WE added to the buffer
        //
        if (had_output)
            Trim_Tail(mo, '\n');

        Drop_Pointer_From_Series(TG_Mold_Stack, c);
        return;
    }

    // Otherwise we are molding

    Pre_Mold(mo, v);

    Append_Codepoint(s, '[');

    mo->indent++;

    const REBKEY *tail;
    const REBKEY *key = CTX_KEYS(&tail, c);
    REBVAR *var = CTX_VARS_HEAD(c);
    REBPAR *param = (CELL_KIND(v) == REB_FRAME)
        ? ACT_PARAMS_HEAD(VAL_FRAME_PHASE(v))
        : cast_PAR(var);

    for (; key != tail; ++key, ++var, ++param) {
        if (honor_hidden and Is_Param_Hidden(param))
            continue;

        New_Indented_Line(mo);

        const REBSTR *spelling = KEY_SYMBOL(key);
        Append_Utf8(s, STR_UTF8(spelling), STR_SIZE(spelling));

        Append_Ascii(s, ": ");

        if (IS_NULLED(var))
            Append_Ascii(s, "'");  // `field: '` would evaluate to null
        else {
            if (IS_VOID(var) or not ANY_INERT(var))  // needs quoting
                Append_Ascii(s, "'");
            Mold_Value(mo, var);
        }
    }

    mo->indent--;
    New_Indented_Line(mo);
    Append_Codepoint(s, ']');

    End_Mold(mo);

    Drop_Pointer_From_Series(TG_Mold_Stack, c);
}


//
//  Context_Common_Action_Maybe_Unhandled: C
//
// Similar to Series_Common_Action_Maybe_Unhandled().  Introduced because
// PORT! wants to act like a context for some things, but if you ask an
// ordinary object if it's OPEN? it doesn't know how to do that.
//
REB_R Context_Common_Action_Maybe_Unhandled(
    REBFRM *frame_,
    const REBVAL *verb
){
    REBVAL *v = D_ARG(1);
    REBCTX *c = VAL_CONTEXT(v);

    switch (VAL_WORD_ID(verb)) {
      case SYM_REFLECT: {
        INCLUDE_PARAMS_OF_REFLECT;
        UNUSED(ARG(value));  // covered by `v`

        REBVAL *property = ARG(property);
        switch (VAL_WORD_ID(property)) {
          case SYM_LENGTH: // !!! Should this be legal?
            return Init_Integer(D_OUT, CTX_LEN(c));

          case SYM_TAIL_Q: // !!! Should this be legal?
            return Init_Logic(D_OUT, CTX_LEN(c) == 0);

          case SYM_WORDS:
            return Init_Block(D_OUT, Context_To_Array(v, 1));

          case SYM_VALUES:
            return Init_Block(D_OUT, Context_To_Array(v, 2));

          case SYM_BODY:
            return Init_Block(D_OUT, Context_To_Array(v, 3));

        // Noticeably not handled by average objects: SYM_OPEN_Q (`open?`)

          default:
            break;
        }

        return R_UNHANDLED; }

      default:
        break;
    }

    return R_UNHANDLED;
}


//
//  REBTYPE: C
//
// Handles object!, module!, and error! datatypes.
//
REBTYPE(Context)
{
    REB_R r = Context_Common_Action_Maybe_Unhandled(frame_, verb);
    if (r != R_UNHANDLED)
        return r;

    REBVAL *context = D_ARG(1);
    REBCTX *c = VAL_CONTEXT(context);

    switch (VAL_WORD_ID(verb)) {
      case SYM_REFLECT: {
        INCLUDE_PARAMS_OF_REFLECT;
        UNUSED(ARG(value));  // covered by `v`

        if (VAL_TYPE(context) != REB_FRAME)
            break;

        REBVAL *property = ARG(property);
        SYMID sym = VAL_WORD_ID(property);

        if (sym == SYM_LABEL) {
            //
            // Can be answered for frames that have no execution phase, if
            // they were initialized with a label.
            //
            option(const REBSYM*) label = VAL_FRAME_LABEL(context);
            if (label)
                return Init_Word(D_OUT, unwrap(label));

            // If the frame is executing, we can look at the label in the
            // REBFRM*, which will tell us what the overall execution label
            // would be.  This might be confusing, however...if the phase
            // is drastically different.  Review.
        }

        if (sym == SYM_ACTION) {
            //
            // Currently this can be answered for any frame, even if it is
            // expired...though it probably shouldn't do this unless it's
            // an indefinite lifetime object, so that paramlists could be
            // GC'd if all the frames pointing to them were expired but still
            // referenced somewhere.
            //
            return Init_Action(
                D_OUT,
                VAL_FRAME_PHASE(context),  // just a REBACT*, no binding
                VAL_FRAME_LABEL(context),
                VAL_FRAME_BINDING(context)  // e.g. where RETURN returns to
            );
        }

        REBFRM *f = CTX_FRAME_MAY_FAIL(c);

        switch (sym) {
          case SYM_FILE: {
            const REBSTR *file = FRM_FILE(f);
            if (not file)
                return nullptr;
            return Init_File(D_OUT, file); }

          case SYM_LINE: {
            REBLIN line = FRM_LINE(f);
            if (line == 0)
                return nullptr;
            return Init_Integer(D_OUT, line); }

          case SYM_LABEL: {
            if (not f->label)
                return nullptr;
            return Init_Word(D_OUT, unwrap(f->label)); }

          case SYM_NEAR:
            return Init_Near_For_Frame(D_OUT, f);

          case SYM_PARENT: {
            //
            // Only want action frames (though `pending? = true` ones count).
            //
            REBFRM *parent = f;
            while ((parent = parent->prior) != FS_BOTTOM) {
                if (not Is_Action_Frame(parent))
                    continue;

                REBCTX* ctx_parent = Context_For_Frame_May_Manage(parent);
                RETURN (CTX_ARCHETYPE(ctx_parent));
            }
            return nullptr; }

          default:
            break;
        }
        fail (Error_Cannot_Reflect(VAL_TYPE(context), property)); }


      case SYM_APPEND: {
        REBVAL *arg = D_ARG(2);
        if (IS_NULLED_OR_BLANK(arg))
            RETURN (context);  // don't fail on R/O if it would be a no-op

        ENSURE_MUTABLE(context);
        if (not IS_OBJECT(context) and not IS_MODULE(context))
            return R_UNHANDLED;
        Append_To_Context(context, arg);
        RETURN (context); }

      case SYM_COPY: {  // Note: words are not copied and bindings not changed!
        INCLUDE_PARAMS_OF_COPY;
        UNUSED(PAR(value));  // covered by `context`

        if (REF(part))
            fail (Error_Bad_Refines_Raw());

        REBU64 types = 0;
        if (REF(types)) {
            if (IS_DATATYPE(ARG(types)))
                types = FLAGIT_KIND(VAL_TYPE_KIND(ARG(types)));
            else {
                types |= VAL_TYPESET_LOW_BITS(ARG(types));
                types |= cast(REBU64, VAL_TYPESET_HIGH_BITS(ARG(types))) << 32;
            }
        }
        else if (REF(deep))
            types = TS_STD_SERIES;

        // !!! Special attention on copying frames is going to be needed,
        // because copying a frame will be expected to create a new identity
        // for an ACTION! if that frame is aliased AS ACTION!.  The design
        // of this is still evolving, but we don't want archetypal values
        // otherwise we could not `do copy f`, so initialize with label.
        //
        if (IS_FRAME(context)) {
            return Init_Frame(
                D_OUT,
                Copy_Context_Extra_Managed(c, 0, types),
                VAL_FRAME_LABEL(context)
            );
        }

        return Init_Any_Context(
            D_OUT,
            VAL_TYPE(context),
            Copy_Context_Extra_Managed(c, 0, types)
        ); }

      case SYM_SELECT:
      case SYM_FIND: {
        INCLUDE_PARAMS_OF_FIND;
        UNUSED(ARG(series));  // extracted as `c`
        UNUSED(ARG(part));
        UNUSED(ARG(only));
        UNUSED(ARG(skip));
        UNUSED(ARG(tail));
        UNUSED(ARG(match));
        UNUSED(ARG(reverse));
        UNUSED(ARG(last));

        REBVAL *pattern = ARG(pattern);
        if (not IS_WORD(pattern))
            return nullptr;

        REBLEN n = Find_Symbol_In_Context(
            context,
            VAL_WORD_SYMBOL(pattern),
            did REF(case)
        );
        if (n == 0)
            return nullptr;

        if (VAL_WORD_ID(verb) == SYM_FIND)
            return Init_True(D_OUT); // !!! obscures non-LOGIC! result?

        RETURN (CTX_VAR(c, n)); }

      default:
        break;
    }

    return R_UNHANDLED;
}


//
//  construct: native [
//
//  "Creates an ANY-CONTEXT! instance"
//
//      return: [<opt> any-context!]
//      spec [<blank> block!]
//          "Object specification block (bindings modified)"
//      /only "Values are kept as-is"
//      /with "Use a parent/prototype context"
//          [any-context!]
//  ]
//
REBNATIVE(construct)
//
// !!! This assumes you want a SELF defined.  The entire concept of SELF
// needs heavy review.
//
// !!! This mutates the bindings of the spec block passed in, should it
// be making a copy instead (at least by default, perhaps with performance
// junkies saying `construct/rebind` or something like that?
//
// !!! /ONLY should be done with a "predicate", e.g. `construct .quote [...]`
{
    INCLUDE_PARAMS_OF_CONSTRUCT;

    REBVAL *spec = ARG(spec);
    REBCTX *parent = REF(with) ? VAL_CONTEXT(ARG(with)) : nullptr;

    // This parallels the code originally in CONSTRUCT.  Run it if the /ONLY
    // refinement was passed in.
    //
  blockscope {
    const RELVAL *tail;
    RELVAL *at = VAL_ARRAY_AT_MUTABLE_HACK(&tail, spec);
    if (REF(only)) {
        Init_Object(
            D_OUT,
            Construct_Context_Managed(
                REB_OBJECT,
                at,  // warning: modifies binding!
                tail,
                VAL_SPECIFIER(spec),
                parent
            )
        );
        return D_OUT;
    }
  }

    // Scan the object for top-level set words in order to make an
    // appropriately sized context.
    //
    const RELVAL *tail;
    RELVAL *at = VAL_ARRAY_AT_ENSURE_MUTABLE(&tail, spec);

    REBCTX *ctx = Make_Context_Detect_Managed(
        parent ? CTX_TYPE(parent) : REB_OBJECT,  // !!! Presume object?
        at,
        tail,
        parent
    );
    Init_Object(D_OUT, ctx);  // GC protects context

    // !!! This binds the actual body data, not a copy of it.  See
    // Virtual_Bind_Deep_To_New_Context() for future directions.
    //
    Bind_Values_Deep(at, tail, CTX_ARCHETYPE(ctx));

    DECLARE_LOCAL (dummy);
    if (Do_Any_Array_At_Throws(dummy, spec, SPECIFIED)) {
        Move_Cell(D_OUT, dummy);
        return R_THROWN;  // evaluation result ignored unless thrown
    }

    return D_OUT;
}
