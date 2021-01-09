//
//  File: %c-function.c
//  Summary: "support for functions, actions, and routines"
//  Section: core
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
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
// Licensed under the Lesser GPL, Version 3.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.gnu.org/licenses/lgpl-3.0.html
//
//=////////////////////////////////////////////////////////////////////////=//
//

#include "sys-core.h"


struct Params_Of_State {
    bool just_words;
};

// Reconstitute parameter back into a full value, e.g. REB_P_REFINEMENT
// becomes `/spelling`.
//
// !!! See notes on Is_Param_Hidden() for why caller isn't filtering locals.
//
static bool Params_Of_Hook(
    const REBKEY *key,
    const REBPAR *param,
    REBFLGS flags,
    void *opaque
){
    struct Params_Of_State *s = cast(struct Params_Of_State*, opaque);

    Init_Word(DS_PUSH(), KEY_SPELLING(key));

    if (not s->just_words) {
        if (
            not (flags & PHF_UNREFINED)
            and TYPE_CHECK(param, REB_TS_REFINEMENT)
        ){
            Refinify(DS_TOP);
        }

        switch (VAL_PARAM_CLASS(param)) {
          case REB_P_OUTPUT:
          case REB_P_NORMAL:
            break;

          case REB_P_MODAL:
            if (flags & PHF_DEMODALIZED) {
                // associated refinement specialized out
            }
            else
                Symify(DS_TOP);
            break;

          case REB_P_SOFT:
            Getify(DS_TOP);
            break;

          case REB_P_MEDIUM:
            Quotify(Getify(DS_TOP), 1);
            break;

          case REB_P_HARD:
            Quotify(DS_TOP, 1);
            break;

          default:
            assert(false);
            DEAD_END;
        }
    }

    return true;
}

//
//  Make_Action_Parameters_Arr: C
//
// Returns array of function words, unbound.
//
REBARR *Make_Action_Parameters_Arr(REBACT *act, bool just_words)
{
    struct Params_Of_State s;
    s.just_words = just_words;

    REBDSP dsp_orig = DSP;
    For_Each_Unspecialized_Param(act, &Params_Of_Hook, &s);
    return Pop_Stack_Values(dsp_orig);
}


static bool Typesets_Of_Hook(
    const REBKEY *key,
    const REBPAR *param,
    REBFLGS flags,
    void *opaque
){
    UNUSED(key);
    UNUSED(opaque);
    UNUSED(flags);

    // !!! Typesets must be revisited in a world with user-defined types, as
    // well as to accomodate multiple quoting levels.
    //
    Move_Value(DS_PUSH(), param);
    assert(IS_TYPESET(DS_TOP));
    assert(VAL_TYPESET_STRING_NODE(DS_TOP) == nullptr);

    return true;
}

//
//  Make_Action_Typesets_Arr: C
//
// Return a block of function arg typesets.
// Note: skips 0th entry.
//
REBARR *Make_Action_Typesets_Arr(REBACT *act)
{
    void *opaque = nullptr;  // could be any void* to pass thru to hook

    REBDSP dsp_orig = DSP;
    For_Each_Unspecialized_Param(act, &Typesets_Of_Hook, opaque);
    return Pop_Stack_Values(dsp_orig);
}


enum Reb_Spec_Mode {
    SPEC_MODE_NORMAL, // words are arguments
    SPEC_MODE_LOCAL, // words are locals
    SPEC_MODE_WITH // words are "extern"
};


//
//  Push_Paramlist_Triads_May_Fail: C
//
// This is an implementation routine for Make_Paramlist_Managed_May_Fail().
// It was broken out into its own separate routine so that the AUGMENT
// function could reuse the logic for function spec analysis.  It may not
// be broken out in a particularly elegant way, but it's a start.
//
void Push_Paramlist_Triads_May_Fail(
    const REBVAL *spec,
    REBFLGS *flags,
    REBDSP dsp_orig,
    REBDSP *definitional_return_dsp
){
    assert(IS_BLOCK(spec));

    enum Reb_Spec_Mode mode = SPEC_MODE_NORMAL;

    bool refinement_seen = false;

    const RELVAL* value = VAL_ARRAY_AT(spec);

    while (NOT_END(value)) {
        const RELVAL* item = value;  // "faked"
        ++value;  // go ahead and consume next

    //=//// STRING! FOR FUNCTION DESCRIPTION OR PARAMETER NOTE ////////////=//

        if (IS_TEXT(item)) {
            //
            // Consider `[<with> some-extern "description of that extern"]` to
            // be purely commentary for the implementation, and don't include
            // it in the meta info.
            //
            if (mode == SPEC_MODE_WITH)
                continue;

            if (IS_PARAM(DS_TOP) or IS_WORD(DS_TOP))  // word for locals
                Move_Value(DS_PUSH(), EMPTY_BLOCK);  // need block in position

            if (IS_BLOCK(DS_TOP)) {  // in right spot to push notes/title
                Init_Text(DS_PUSH(), Copy_String_At(item));
            }
            else {  // !!! A string was already pushed.  Should we append?
                assert(IS_TEXT(DS_TOP));
                Init_Text(DS_TOP, Copy_String_At(item));
            }

            if (DS_TOP == DS_AT(dsp_orig + 3))
                *flags |= MKF_HAS_DESCRIPTION;
            else
                *flags |= MKF_HAS_NOTES;

            continue;
        }

    //=//// TOP-LEVEL SPEC TAGS LIKE <local>, <with> etc. /////////////////=//

        bool strict = false;
        if (IS_TAG(item) and (*flags & MKF_KEYWORDS)) {
            if (0 == CT_String(item, Root_With_Tag, strict)) {
                mode = SPEC_MODE_WITH;
                continue;
            }
            else if (0 == CT_String(item, Root_Local_Tag, strict)) {
                mode = SPEC_MODE_LOCAL;
                continue;
            }
            else if (0 == CT_String(item, Root_Void_Tag, strict)) {
                *flags |= MKF_IS_VOIDER;  // use Voider_Dispatcher()

                // Fake as if they said [void!] !!! make more efficient
                //
                item = Get_System(SYS_STANDARD, STD_PROC_RETURN_TYPE);
                goto process_typeset_block;
            }
            else if (0 == CT_String(item, Root_Elide_Tag, strict)) {
                *flags |= MKF_IS_ELIDER;

                // Fake as if they said [<invisible>] !!! make more efficient
                //
                item = Get_System(SYS_STANDARD, STD_ELIDER_RETURN_TYPE);
                goto process_typeset_block;
            }
            else
                fail (Error_Bad_Func_Def_Core(item, VAL_SPECIFIER(spec)));
        }

    //=//// BLOCK! OF TYPES TO MAKE TYPESET FROM (PLUS PARAMETER TAGS) ////=//

        if (IS_BLOCK(item)) {
          process_typeset_block:
            if (IS_BLOCK(DS_TOP)) // two blocks of types!
                fail (Error_Bad_Func_Def_Core(item, VAL_SPECIFIER(spec)));

            // You currently can't say `<local> x [integer!]`, because they
            // are always void when the function runs.  You can't say
            // `<with> x [integer!]` because "externs" don't have param slots
            // to store the type in.
            //
            // !!! A type constraint on a <with> parameter might be useful,
            // though--and could be achieved by adding a type checker into
            // the body of the function.  However, that would be more holistic
            // than this generation of just a paramlist.  Consider for future.
            //
            if (mode != SPEC_MODE_NORMAL)
                fail (Error_Bad_Func_Def_Core(item, VAL_SPECIFIER(spec)));

            // Save the block for parameter types.
            //
            STKVAL(*) param;
            if (IS_PARAM(DS_TOP) or IS_WORD(DS_TOP)) {
                REBSPC* derived = Derive_Specifier(VAL_SPECIFIER(spec), item);
                Init_Block(
                    DS_PUSH(),
                    Copy_Array_At_Deep_Managed(
                        VAL_ARRAY(item),
                        VAL_INDEX(item),
                        derived
                    )
                );

                param = DS_TOP - 1;  // volatile if you DS_PUSH()!
            }
            else {
                assert(IS_TEXT(DS_TOP));  // !!! are blocks after notes good?

                if (IS_VOID_RAW(DS_TOP - 2)) {
                    //
                    // No parameters pushed, e.g. func [[integer!] {<-- bad}]
                    //
                    fail (Error_Bad_Func_Def_Core(item, VAL_SPECIFIER(spec)));
                }

                assert(IS_PARAM(DS_TOP - 2) or IS_WORD(DS_TOP - 2));
                param = DS_TOP - 2;

                assert(IS_BLOCK(DS_TOP - 1));
                if (VAL_ARRAY(DS_TOP - 1) != EMPTY_ARRAY)
                    fail (Error_Bad_Func_Def_Core(item, VAL_SPECIFIER(spec)));

                REBSPC* derived = Derive_Specifier(VAL_SPECIFIER(spec), item);
                Init_Block(
                    DS_TOP - 1,
                    Copy_Array_At_Deep_Managed(
                        VAL_ARRAY(item),
                        VAL_INDEX(item),
                        derived
                    )
                );
            }

            // Turn block into typeset for parameter at current index.
            // Leaves VAL_TYPESET_SYM as-is.
            //
            if (not IS_WORD(param)) {
                bool was_refinement =  TYPE_CHECK(param, REB_TS_REFINEMENT);
                REBSPC* derived = Derive_Specifier(VAL_SPECIFIER(spec), item);
                VAL_TYPESET_LOW_BITS(param) = 0;
                VAL_TYPESET_HIGH_BITS(param) = 0;
                Add_Typeset_Bits_Core(
                    param,
                    ARR_HEAD(VAL_ARRAY(item)),
                    derived
                );
                if (was_refinement)
                    TYPE_SET(param, REB_TS_REFINEMENT);

                *flags |= MKF_HAS_TYPES;
            }
            continue;
        }

    //=//// ANY-WORD! PARAMETERS THEMSELVES (MAKE TYPESETS w/SYMBOL) //////=//

        bool quoted = false;  // single quoting level used as signal in spec
        if (VAL_NUM_QUOTES(item) > 0) {
            if (VAL_NUM_QUOTES(item) > 1)
                fail (Error_Bad_Func_Def_Core(item, VAL_SPECIFIER(spec)));
            quoted = true;
        }

        REBCEL(const*) cell = VAL_UNESCAPED(item);
        enum Reb_Kind kind = CELL_KIND(cell);

        const REBSTR* spelling = nullptr;  // avoids compiler warning
        Reb_Param_Class pclass = REB_P_DETECT;  // error if not changed below

        bool refinement = false;  // paths with blanks at head are refinements
        if (ANY_PATH_KIND(kind)) {
            if (not IS_REFINEMENT_CELL(cell))
                fail (Error_Bad_Func_Def_Core(item, VAL_SPECIFIER(spec)));

            refinement = true;
            refinement_seen = true;

            // !!! If you say [<with> x /foo y] the <with> terminates and a
            // refinement is started.  Same w/<local>.  Is this a good idea?
            // Note that historically, help hides any refinements that appear
            // behind a /local, but this feature has no parallel in Ren-C.
            //
            mode = SPEC_MODE_NORMAL;

            spelling = VAL_REFINEMENT_SPELLING(cell);
            if (STR_SYMBOL(spelling) == SYM_LOCAL)  // /LOCAL
                if (ANY_WORD_KIND(KIND3Q_BYTE(item + 1)))  // END is 0
                    fail (Error_Legacy_Local_Raw(spec));  // -> <local>

            if (CELL_KIND(cell) == REB_GET_PATH) {
                if (quoted)
                    pclass = REB_P_MEDIUM;
                else
                    pclass = REB_P_SOFT;
            }
            else if (CELL_KIND(cell) == REB_PATH) {
                if (quoted)
                    pclass = REB_P_HARD;
                else
                    pclass = REB_P_NORMAL;
            }
        }
        else if (ANY_TUPLE_KIND(kind)) {
            //
            // !!! Tuples are theorized as a way to "name parameters out of
            // the way" so there can be an interface name, but then a local
            // name...so that something like /ALL can be named out of the
            // way without disrupting use of ALL.  That's not implemented yet,
            // but it's now another way to name locals.
            //
            if (IS_PREDICATE1_CELL(cell) and not quoted) {
                pclass = REB_VOID;
                spelling = VAL_PREDICATE1_SPELLING(cell);
            }
        }
        else if (ANY_WORD_KIND(kind)) {
            spelling = VAL_WORD_SPELLING(cell);

            if (kind == REB_SET_WORD) {
                if (VAL_WORD_SYM(cell) == SYM_RETURN and not quoted) {
                    pclass = REB_VOID;
                }
                else if (not quoted) {
                    refinement = true;  // sets REB_TS_REFINEMENT (optional)
                    pclass = REB_P_OUTPUT;
                }
            }
            else {
                if (  // let RETURN: presence indicate you know new rules
                    refinement_seen and mode == SPEC_MODE_NORMAL
                    and *definitional_return_dsp == 0
                ){
                    fail (Error_Legacy_Refinement_Raw(spec));
                }

                if (kind == REB_GET_WORD) {
                    if (quoted)
                        pclass = REB_P_MEDIUM;
                    else
                        pclass = REB_P_SOFT;
                }
                else if (kind == REB_WORD) {
                    if (quoted)
                        pclass = REB_P_HARD;
                    else
                        pclass = REB_P_NORMAL;
                }
                else if (kind == REB_SYM_WORD) {
                    if (not quoted)
                        pclass = REB_P_MODAL;
                }
            }
        }
        else
            fail (Error_Bad_Func_Def_Core(item, VAL_SPECIFIER(spec)));

        if (pclass == REB_P_DETECT)  // didn't match
            fail (Error_Bad_Func_Def_Core(item, VAL_SPECIFIER(spec)));

        if (mode != SPEC_MODE_NORMAL) {
            if (pclass != REB_P_NORMAL and pclass != REB_VOID)
                fail (Error_Bad_Func_Def_Core(item, VAL_SPECIFIER(spec)));

            if (mode == SPEC_MODE_LOCAL)
                pclass = REB_VOID;
        }

        if (STR_SYMBOL(spelling) == SYM_RETURN and pclass != REB_VOID) {
            //
            // Cancel definitional return if any non-SET-WORD! uses the name
            // RETURN when defining a FUNC.
            //
            *flags &= ~MKF_RETURN;
        }

        // Because FUNC does not do any locals gathering by default, the main
        // purpose of tolerating <with> is for instructing it not to do the
        // definitional returns.  However, it also makes changing between
        // FUNC and FUNCTION more fluid.
        //
        // !!! If you write something like `func [x <with> x] [...]` that
        // should be sanity checked with an error...TBD.
        //
        if (mode == SPEC_MODE_WITH)
            continue;

        // In rhythm of TYPESET! BLOCK! TEXT! we want to be on a string spot
        // at the time of the push of each new typeset.
        //
        if (IS_PARAM(DS_TOP) or IS_WORD(DS_TOP))
            Move_Value(DS_PUSH(), EMPTY_BLOCK);
        if (IS_BLOCK(DS_TOP))
            Move_Value(DS_PUSH(), EMPTY_TEXT);
        assert(IS_TEXT(DS_TOP));

        // Non-annotated arguments disallow ACTION!, VOID! and NULL.  Not
        // having to worry about ACTION! and NULL means by default, code
        // does not have to worry about "disarming" arguments via GET-WORD!.
        // Also, keeping NULL a bit "prickly" helps discourage its use as
        // an input parameter...because it faces problems being used in
        // SPECIALIZE and other scenarios.
        //
        // Note there are currently two ways to get NULL: <opt> and <end>.
        // If the typeset bits contain REB_NULL, that indicates <opt>.
        // But Is_Param_Endable() indicates <end>.

        if (pclass == REB_VOID) {
            Init_Word(DS_PUSH(), spelling);
        }
        else if (refinement) {
            Init_Param(
                DS_PUSH(),
                pclass,
                spelling,  // don't canonize, see #2258
                FLAGIT_KIND(REB_TS_REFINEMENT)  // must preserve if type block
            );
        }
        else
            Init_Param(
                DS_PUSH(),
                pclass,
                spelling,  // don't canonize, see #2258
                TS_OPT_VALUE  // By default <opt> ANY-VALUE! is legal
            );

        // All these would cancel a definitional return (leave has same idea):
        //
        //     func [return [integer!]]
        //     func [/refinement return]
        //     func [<local> return]
        //     func [<with> return]
        //
        // ...although `return:` is explicitly tolerated ATM for compatibility
        // (despite violating the "pure locals are NULL" premise)
        //
        if (spelling == Canon(SYM_RETURN)) {
            if (*definitional_return_dsp != 0) {
                DECLARE_LOCAL(word);
                Init_Word(word, spelling);
                fail (Error_Dup_Vars_Raw(word));  // most dup checks are later
            }
            if (pclass == REB_VOID)
                *definitional_return_dsp = DSP;  // RETURN: explicit, tolerate
            else
                *flags &= ~MKF_RETURN;
        }
    }
}


//
//  Pop_Paramlist_And_Meta_May_Fail: C
//
// Assuming the stack is formed in a rhythm of the parameter, a type spec
// block, and a description...produce a paramlist in a state suitable to be
// passed to Make_Action().  It may not succeed because there could be
// duplicate parameters on the stack, and the checking via a binder is done
// as part of this popping process.
//
REBARR *Pop_Paramlist_With_Meta_May_Fail(
    REBCTX **meta,
    REBDSP dsp_orig,
    REBFLGS flags,
    REBDSP definitional_return_dsp
){
    // Go ahead and flesh out the TYPESET! BLOCK! TEXT! triples.
    //
    if (IS_PARAM(DS_TOP) or IS_WORD(DS_TOP))
        Move_Value(DS_PUSH(), EMPTY_BLOCK);
    if (IS_BLOCK(DS_TOP))
        Move_Value(DS_PUSH(), EMPTY_TEXT);
    assert((DSP - dsp_orig) % 3 == 0); // must be a multiple of 3

    // Definitional RETURN slots must have their argument value fulfilled with
    // an ACTION! specific to the action called on *every instantiation*.
    // They are marked with special parameter classes to avoid needing to
    // separately do canon comparison of their symbols to find them.
    //
    // Note: Since RETURN's typeset holds types that need to be checked at
    // the end of the function run, it is moved to a predictable location:
    // first slot of the paramlist.  Initially it was the last slot...but this
    // enables adding more arguments/refinements/locals in derived functions.

    if (flags & MKF_RETURN) {
        if (definitional_return_dsp == 0) { // no explicit RETURN: pure local
            //
            // While default arguments disallow ACTION!, VOID!, and NULL...
            // they are allowed to return anything.  Generally speaking, the
            // checks are on the input side, not the output.
            //
            Init_Word(DS_PUSH(), Canon(SYM_RETURN));
            definitional_return_dsp = DSP;

            Move_Value(DS_PUSH(), EMPTY_BLOCK);
            Move_Value(DS_PUSH(), EMPTY_TEXT);
        }
        else {
            STKVAL(*) param = DS_AT(definitional_return_dsp);
            assert(VAL_WORD_SYM(param) == SYM_RETURN);
            UNUSED(param);
        }

        // definitional_return handled specially when paramlist copied
        // off of the stack...moved to head position.

        flags |= MKF_HAS_RETURN;
    }

    // Slots, which is length +1 (includes the rootvar or rootparam)
    //
    REBLEN num_slots = (DSP - dsp_orig) / 3;

    // There should be no more pushes past this point, so a stable pointer
    // into the stack for the definitional return can be found.
    //
    STKVAL(*) definitional_return =
        definitional_return_dsp == 0
            ? nullptr
            : DS_AT(definitional_return_dsp);

    // Must make the function "paramlist" even if "empty", for identity.
    //
    // !!! In order to facilitate adding to the frame in the copy and
    // relativize step to add LET variables, don't pass SERIES_FLAG_FIXED_SIZE
    // in the creation step.  This formats cells in such a way that the
    // series mechanically cannot be expanded even if the flag is removed.
    // Instead, add it onto a series allocated as resizable.  This is likely
    // temporary--as LET mechanics should use some form of "virtual binding".
    //
    // !!! Note this means that Assert_Array_Core() has to have an exemption
    // for ARRAY_FLAG_IS_PARAMLIST...review that.
    //
    REBARR *paramlist = Make_Array_Core(
        num_slots,
        SERIES_MASK_PARAMLIST & ~(SERIES_FLAG_FIXED_SIZE)
    );
    SET_SERIES_FLAG(paramlist, FIXED_SIZE);

    REBSER *keylist = Make_Series_Core(
        (num_slots - 1) + 1,  // - 1 archetype, + 1 terminator
        sizeof(REBKEY),
        NODE_FLAG_MANAGED | SERIES_MASK_KEYLIST
    );
    LINK_ANCESTOR_NODE(keylist) = NOD(keylist);  // chain ends with self

    // !!!
    // !!! TEMPORARY -- TURNING OFF RETURN CHECKS
    // !!!

/*    if (flags & MKF_HAS_RETURN)
        paramlist->header.bits |= PARAMLIST_FLAG_HAS_RETURN; */

    // We want to check for duplicates and a Binder can be used for that
    // purpose--but note that a fail () cannot happen while binders are
    // in effect UNLESS the BUF_COLLECT contains information to undo it!
    // There's no BUF_COLLECT here, so don't fail while binder in effect.
    //
    // (This is why we wait until the parameter list gathering process
    // is over to do the duplicate checks--it can fail.)
    //
    struct Reb_Binder binder;
    INIT_BINDER(&binder);

    const REBSTR *duplicate = nullptr;

  blockscope {
    REBVAL *param = Voidify_Rootparam(paramlist) + 1;
    REBKEY *key = SER_HEAD(REBKEY, keylist);

    STKVAL(*) src = DS_AT(dsp_orig + 1) + 3;

    if (definitional_return) {
        assert(flags & MKF_RETURN);
        Init_Key(key, VAL_WORD_SPELLING(definitional_return));
        ++key;

        Init_Void(param, SYM_UNSET);  // acts as a local !!! being revisited
        SET_CELL_FLAG(param, ARG_MARKED_CHECKED);
        ++param;
    }

    // Weird due to Spectre/MSVC: https://stackoverflow.com/q/50399940
    //
    for (; src != DS_TOP + 1; src += 3) {
        const REBSTR *spelling;
        if (IS_WORD(src))
            spelling = VAL_WORD_SPELLING(src);
        else
            spelling = VAL_TYPESET_STRING(src);

        if (not Is_Param_Sealed(cast_PAR(src))) {  // sealed names reusable
            if (not Try_Add_Binder_Index(&binder, spelling, 1020))
                duplicate = spelling;
        }

        if (definitional_return and src == definitional_return)
            continue;

        if (IS_WORD(src)) {  // e.g. a local
            Init_Key(key, VAL_WORD_SPELLING(src));

            Init_Void(param, SYM_UNSET);
            SET_CELL_FLAG(param, ARG_MARKED_CHECKED);
        }
        else {
            Init_Key(key, VAL_TYPESET_STRING(src));

            Move_Value(param, src);
            VAL_TYPESET_STRING_NODE(param) = nullptr;
        }
        ++key;
        ++param;
    }

    TERM_ARRAY_LEN(paramlist, num_slots);
    Manage_Series(paramlist);

    TERM_SEQUENCE_LEN(keylist, ARR_LEN(paramlist) - 1);
    INIT_LINK_KEYSOURCE(paramlist, NOD(keylist));
    MISC_META_NODE(paramlist) = nullptr;
  }

    // Must remove binder indexes for all words, even if about to fail
    //
  blockscope {
    const REBKEY *key = SER_AT(REBKEY, keylist, 0);
    const REBPAR *param = SER_AT(REBPAR, paramlist, 1);
    for (; NOT_END_KEY(key); ++key, ++param) {
        if (Is_Param_Sealed(param))
            continue;
        if (Remove_Binder_Index_Else_0(&binder, KEY_SPELLING(key)) == 0)
            assert(duplicate);  // erroring on this is pending
    }

    SHUTDOWN_BINDER(&binder);

    if (duplicate) {
        DECLARE_LOCAL (word);
        Init_Word(word, duplicate);
        fail (Error_Dup_Vars_Raw(word));
    }
  }

    //=///////////////////////////////////////////////////////////////////=//
    //
    // BUILD META INFORMATION OBJECT (IF NEEDED)
    //
    //=///////////////////////////////////////////////////////////////////=//

    // !!! See notes on ACTION-META in %sysobj.r

    if (flags & (MKF_HAS_DESCRIPTION | MKF_HAS_TYPES | MKF_HAS_NOTES))
        *meta = Copy_Context_Shallow_Managed(VAL_CONTEXT(Root_Action_Meta));
    else
        *meta = nullptr;

    // If a description string was gathered, it's sitting in the first string
    // slot, the third cell we pushed onto the stack.  Extract it if so.
    //
    if (flags & MKF_HAS_DESCRIPTION) {
        assert(IS_TEXT(DS_AT(dsp_orig + 3)));
        Move_Value(
            CTX_VAR(*meta, STD_ACTION_META_DESCRIPTION),
            DS_AT(dsp_orig + 3)
        );
    }

    // Only make `parameter-types` if there were blocks in the spec
    //
    if (flags & MKF_HAS_TYPES) {
        REBARR *types_varlist = Make_Array_Core(
            num_slots,
            SERIES_MASK_VARLIST | NODE_FLAG_MANAGED
        );
        MISC_META_NODE(types_varlist) = nullptr;  // GC sees, must initialize
        INIT_CTX_KEYLIST_SHARED(CTX(types_varlist), keylist);

        RELVAL *rootvar = ARR_HEAD(types_varlist);
        INIT_VAL_CONTEXT_ROOTVAR(rootvar, REB_OBJECT, types_varlist);

        REBVAL *dest = SPECIFIC(rootvar) + 1;
        const RELVAL *param = ARR_AT(paramlist, 1);

        STKVAL(*) src = DS_AT(dsp_orig + 2);
        src += 3;

        if (definitional_return) {
            //
            // We put the return note in the top-level meta information, not
            // on the local itself (the "return-ness" is a distinct property
            // of the function from what word is used for RETURN:, and it
            // is possible to use the word RETURN for a local or refinement
            // argument while having nothing to do with the exit value of
            // the function.)
            //
            if (NOT_END(VAL_ARRAY_AT(definitional_return + 1))) {
                Move_Value(
                    CTX_VAR(*meta, STD_ACTION_META_RETURN_TYPE),
                    &definitional_return[1]
                );
            }

            Init_Nulled(dest); // clear the local RETURN: var's description
            SET_CELL_FLAG(dest, ARG_MARKED_CHECKED);
            ++dest;
            ++param;
        }

        for (; src <= DS_TOP; src += 3) {
            assert(IS_BLOCK(src));
            if (definitional_return and src == definitional_return + 1)
                continue;

            if (IS_END(VAL_ARRAY_AT(src)))
                Init_Nulled(dest);
            else
                Move_Value(dest, src);

            if (GET_CELL_FLAG(param, ARG_MARKED_CHECKED))
                SET_CELL_FLAG(dest, ARG_MARKED_CHECKED);
            ++dest;
            ++param;
        }
        assert(IS_END(param));

        TERM_ARRAY_LEN(types_varlist, num_slots);

        Init_Object(
            CTX_VAR(*meta, STD_ACTION_META_PARAMETER_TYPES),
            CTX(types_varlist)
        );
    }

    // Only make `parameter-notes` if there were strings (besides description)
    //
    if (flags & MKF_HAS_NOTES) {
        REBARR *notes_varlist = Make_Array_Core(
            num_slots,
            SERIES_MASK_VARLIST | NODE_FLAG_MANAGED
        );
        MISC_META_NODE(notes_varlist) = nullptr;  // GC sees, must initialize
        INIT_CTX_KEYLIST_SHARED(CTX(notes_varlist), keylist);

        RELVAL *rootvar = ARR_HEAD(notes_varlist);
        INIT_VAL_CONTEXT_ROOTVAR(rootvar, REB_OBJECT, notes_varlist); 

        const RELVAL *param = ARR_AT(paramlist, 1);
        REBVAL *dest = SPECIFIC(rootvar) + 1;

        STKVAL(*) src = DS_AT(dsp_orig + 3);
        src += 3;

        if (definitional_return) {
            //
            // See remarks on the return type--the RETURN is documented in
            // the top-level META-OF, not the "incidentally" named RETURN
            // parameter in the list
            //
            if (VAL_LEN_HEAD(definitional_return + 2) == 0)
                Init_Nulled(CTX_VAR(*meta, STD_ACTION_META_RETURN_NOTE));
            else {
                Move_Value(
                    CTX_VAR(*meta, STD_ACTION_META_RETURN_NOTE),
                    &definitional_return[2]
                );
            }

            Init_Nulled(dest);
            SET_CELL_FLAG(dest, ARG_MARKED_CHECKED);
            ++dest;
            ++param;
        }

        for (; src <= DS_TOP; src += 3) {
            assert(IS_TEXT(src));
            if (definitional_return and src == definitional_return + 2)
                continue;

            if (VAL_LEN_HEAD(src) == 0)
                Init_Nulled(dest);
            else
                Move_Value(dest, src);

            if (GET_CELL_FLAG(param, ARG_MARKED_CHECKED))
                SET_CELL_FLAG(dest, ARG_MARKED_CHECKED);
            ++dest;
            ++param;
        }
        assert(IS_END(param));

        TERM_ARRAY_LEN(notes_varlist, num_slots);

        Init_Object(
            CTX_VAR(*meta, STD_ACTION_META_PARAMETER_NOTES),
            CTX(notes_varlist)
        );
    }

  #ifdef DEBUG_EXTANT_STACK_POINTERS
    definitional_return = nullptr;  // holds a count otherwise
  #endif

    // With all the values extracted from stack to array, restore stack pointer
    //
    DS_DROP_TO(dsp_orig);

    return paramlist;
}


//
//  Make_Paramlist_Managed_May_Fail: C
//
// Check function spec of the form:
//
//     ["description" arg "notes" [type! type2! ...] /ref ...]
//
// !!! The spec language was not formalized in R3-Alpha.  Strings were left
// in and it was HELP's job (and any other clients) to make sense of it, e.g.:
//
//     [foo [type!] {doc string :-)}]
//     [foo {doc string :-/} [type!]]
//     [foo {doc string1 :-/} {doc string2 :-(} [type!]]
//
// Ren-C breaks this into two parts: one is the mechanical understanding of
// MAKE ACTION! for parameters in the evaluator.  Then it is the job
// of a generator to tag the resulting function with a "meta object" with any
// descriptions.  As a proxy for the work of a usermode generator, this
// routine tries to fill in FUNCTION-META (see %sysobj.r) as well as to
// produce a paramlist suitable for the function.
//
// Note a "true local" (indicated by a set-word) is considered to be tacit
// approval of wanting a definitional return by the generator.  This helps
// because Red's model for specifying returns uses a SET-WORD!
//
//     func [return: [integer!] {returns an integer}]
//
// In Ren-C's case it just means you want a local called return, but the
// generator will be "initializing it with a definitional return" for you.
// You don't have to use it if you don't want to...and may overwrite the
// variable.  But it won't be a void at the start.
//
// Note: While paramlists should ultimately carry SERIES_FLAG_FIXED_SIZE,
// the product of this routine might need to be added to.  And series that
// are created fixed size have special preparation such that they will trip
// more asserts.  So the fixed size flag is *not* added here, but ensured
// in the Make_Action() step.
//
REBARR *Make_Paramlist_Managed_May_Fail(
    REBCTX **meta,
    const REBVAL *spec,
    REBFLGS *flags  // flags may be modified to carry additional information
){
    REBDSP dsp_orig = DSP;
    assert(DS_TOP == DS_AT(dsp_orig));

    REBDSP definitional_return_dsp = 0;

    // As we go through the spec block, we push TYPESET! BLOCK! TEXT! triples.
    // These will be split out into separate arrays after the process is done.
    // The first slot of the paramlist needs to be the function canon value,
    // while the other two first slots need to be rootkeys.  Get the process
    // started right after a BLOCK! so it's willing to take a string for
    // the function description--it will be extracted from the slot before
    // it is turned into a rootkey for param_notes.
    //
    Init_Unreadable_Void(DS_PUSH()); // paramlist[0] becomes ACT_ARCHETYPE()
    Move_Value(DS_PUSH(), EMPTY_BLOCK); // param_types[0] (object canon)
    Move_Value(DS_PUSH(), EMPTY_TEXT); // param_notes[0] (desc, then canon)

    // The process is broken up into phases so that the spec analysis code
    // can be reused in AUGMENT.
    //
    Push_Paramlist_Triads_May_Fail(
        spec,
        flags,
        dsp_orig,
        &definitional_return_dsp
    );
    REBARR *paramlist = Pop_Paramlist_With_Meta_May_Fail(
        meta,
        dsp_orig,
        *flags,
        definitional_return_dsp
    );

    return paramlist;
}



//
//  Find_Param_Index: C
//
// Find function param word in function "frame".
//
// !!! This is semi-redundant with similar functions for Find_Word_In_Array
// and key finding for objects, review...
//
REBLEN Find_Param_Index(REBARR *paramlist, REBSTR *spelling)
{
    const REBKEY *key = SER_AT(const REBKEY, paramlist, 1);
    REBLEN len = ARR_LEN(paramlist);

    REBLEN n;
    for (n = 1; n < len; ++n, ++key) {
        if (spelling == KEY_SPELLING(key))
            return n;
    }

    return 0;
}


//
//  Make_Action: C
//
// Create an archetypal form of a function, given C code implementing a
// dispatcher that will be called by Eval_Core.  Dispatchers are of the form:
//
//     const REBVAL *Dispatcher(REBFRM *f) {...}
//
// The REBACT returned is "archetypal" because individual REBVALs which hold
// the same REBACT may differ in a per-REBVAL "binding".  (This is how one
// RETURN is distinguished from another--the binding data stored in the REBVAL
// identifies the pointer of the FRAME! to exit).
//
// Actions have an associated REBARR of data, accessible via ACT_DETAILS().
// This is where they can store information that will be available when the
// dispatcher is called.
//
// The `specialty` argument is an interface structure that holds information
// that can be shared between function instances.  It encodes information
// about the parameter names and types, specialization data, as well as any
// partial specialization or parameter reordering instructions.  This can
// take several forms depending on how much detail there is.  See the
// ACT_SPECIALTY() definition for more information on how this is laid out.
//
REBACT *Make_Action(
    REBARR *specialty,  // paramlist, exemplar, partials -> exemplar/paramlist
    REBCTX *meta,
    REBNAT dispatcher, // native C function called by Eval_Core
    REBLEN details_capacity // capacity of ACT_DETAILS (including archetype)
){
    assert(details_capacity >= 1);  // must have room for archetype

    REBARR *paramlist;
    if (GET_ARRAY_FLAG(specialty, IS_PARTIALS)) {
        paramlist = ARR(LINK_PARTIALS_EXEMPLAR_NODE(specialty));
    }
    else {
        assert(GET_ARRAY_FLAG(specialty, IS_VARLIST));
        paramlist = specialty;
    }

    assert(GET_SERIES_FLAG(paramlist, MANAGED));
    assert(
        IS_UNREADABLE_DEBUG(ARR_HEAD(paramlist))  // must fill in
        or CTX_TYPE(CTX(paramlist)) == REB_FRAME
    );

    // !!! There used to be more validation code needed here when it was
    // possible to pass a specialization frame separately from a paramlist.
    // But once paramlists were separated out from the function's identity
    // array (using ACT_DETAILS() as the identity instead of ACT_KEYLIST())
    // then all the "shareable" information was glommed up minus redundancy
    // into the ACT_SPECIALTY().  Here's some of the residual checking, as
    // a placeholder for more useful consistency checking which might be done.
    //
  blockscope {
    REBSER *keylist = SER(LINK_KEYSOURCE(paramlist));

    ASSERT_SERIES_MANAGED(keylist);  // paramlists/keylists, can be shared
    assert(SER_USED(keylist) + 1 == ARR_LEN(paramlist));
    if (paramlist->header.bits & PARAMLIST_FLAG_HAS_RETURN) {
        const REBKEY *key = SER_AT(const REBKEY, paramlist, 0);
        assert(KEY_SYM(key) == SYM_RETURN);
        UNUSED(key);
    }
  }

    // "details" for an action is an array of cells which can be anything
    // the dispatcher understands it to be, by contract.  Terminate it
    // at the given length implicitly.
    //
    REBARR *details = Make_Array_Core(
        details_capacity,  // leave room for archetype
        SERIES_MASK_DETAILS | NODE_FLAG_MANAGED
    );
    RELVAL *archetype = ARR_HEAD(details);
    RESET_CELL(archetype, REB_ACTION, CELL_MASK_ACTION);
    VAL_ACTION_DETAILS_NODE(archetype) = NOD(details);
    VAL_ACTION_BINDING_NODE(archetype) = UNBOUND;

  #if !defined(NDEBUG)  // notice attempted mutation of the archetype cell
    SET_CELL_FLAG(archetype, PROTECTED);
  #endif

    // Leave rest of the cells in the capacity uninitialized (caller fills in)
    //
    TERM_ARRAY_LEN(details, details_capacity);

    LINK(details).dispatcher = dispatcher; // level of indirection, hijackable
    MISC_META_NODE(details) = NOD(meta);

    VAL_ACTION_SPECIALTY_OR_LABEL_NODE(archetype) = NOD(specialty);

    assert(NOT_ARRAY_FLAG(details, HAS_FILE_LINE_UNMASKED));

    REBACT *act = ACT(details); // now it's a legitimate REBACT

    // !!! We may have to initialize the exemplar rootvar.
    //
    REBVAL *rootvar = SER_HEAD(REBVAL, paramlist);
    if (IS_UNREADABLE_DEBUG(rootvar)) {
        INIT_VAL_FRAME_ROOTVAR(rootvar, paramlist, act, UNBOUND);
    }

    // Precalculate cached function flags.  This involves finding the first
    // unspecialized argument which would be taken at a callsite, which can
    // be tricky to figure out with partial refinement specialization.  So
    // the work of doing that is factored into a routine (`PARAMETERS OF`
    // uses it as well).

    const REBPAR *first = First_Unspecialized_Param(nullptr, act);
    if (first) {
        switch (VAL_PARAM_CLASS(first)) {
          case REB_P_OUTPUT:
          case REB_P_NORMAL:
            break;

          case REB_P_MODAL:
          case REB_P_SOFT:
          case REB_P_MEDIUM:
          case REB_P_HARD:
            SET_ACTION_FLAG(act, QUOTES_FIRST);
            break;

          default:
            assert(false);
        }

        if (TYPE_CHECK(first, REB_TS_SKIPPABLE))
            SET_ACTION_FLAG(act, SKIPPABLE_FIRST);
    }

    return act;
}


//
//  Make_Expired_Frame_Ctx_Managed: C
//
// FUNC/PROC bodies contain relative words and relative arrays.  Arrays from
// this relativized body may only be put into a specified REBVAL once they
// have been combined with a frame.
//
// Reflection asks for action body data, when no instance is called.  Hence
// a REBVAL must be produced somehow.  If the body is being copied, then the
// option exists to convert all the references to unbound...but this isn't
// representative of the actual connections in the body.
//
// There could be an additional "archetype" state for the relative binding
// machinery.  But making a one-off expired frame is an inexpensive option.
//
REBCTX *Make_Expired_Frame_Ctx_Managed(REBACT *a)
{
    // Since passing SERIES_MASK_VARLIST includes SERIES_FLAG_ALWAYS_DYNAMIC,
    // don't pass it in to the allocation...it needs to be set, but will be
    // overridden by SERIES_INFO_INACCESSIBLE.
    //
    REBARR *varlist = Alloc_Singular(NODE_FLAG_MANAGED);
    varlist->header.bits |= SERIES_MASK_VARLIST;
    SET_SERIES_INFO(varlist, INACCESSIBLE);
    MISC_META_NODE(varlist) = nullptr;

    RELVAL *rootvar = ARR_SINGLE(varlist);
    INIT_VAL_FRAME_ROOTVAR(rootvar, varlist, a, UNBOUND);  // !!! binding?

    REBCTX *expired = CTX(varlist);
    INIT_CTX_KEYLIST_SHARED(expired, ACT_KEYLIST(a));

    return expired;
}


//
//  Get_Maybe_Fake_Action_Body: C
//
// !!! While the interface as far as the evaluator is concerned is satisfied
// with the OneAction ACTION!, the various dispatchers have different ideas
// of what "source" would be like.  There should be some mapping from the
// dispatchers to code to get the BODY OF an ACTION.  For the moment, just
// handle common kinds so the SOURCE command works adquately, revisit later.
//
void Get_Maybe_Fake_Action_Body(REBVAL *out, const REBVAL *action)
{
    REBCTX *binding = VAL_ACTION_BINDING(action);
    REBACT *a = VAL_ACTION(action);

    // A Hijacker *might* not need to splice itself in with a dispatcher.
    // But if it does, bypass it to get to the "real" action implementation.
    //
    // !!! Should the source inject messages like {This is a hijacking} at
    // the top of the returned body?
    //
    while (ACT_DISPATCHER(a) == &Hijacker_Dispatcher) {
        a = VAL_ACTION(ARR_AT(ACT_DETAILS(a), 1));
        // !!! Review what should happen to binding
    }

    REBARR *details = ACT_DETAILS(a);

    // !!! Should the binding make a difference in the returned body?  It is
    // exposed programmatically via CONTEXT OF.
    //
    UNUSED(binding);

    if (
        ACT_DISPATCHER(a) == &Void_Dispatcher
        or ACT_DISPATCHER(a) == &Empty_Dispatcher
        or ACT_DISPATCHER(a) == &Unchecked_Dispatcher
        or ACT_DISPATCHER(a) == &Voider_Dispatcher
        or ACT_DISPATCHER(a) == &Returner_Dispatcher
        or ACT_DISPATCHER(a) == &Block_Dispatcher
    ){
        // Interpreted code, the body is a block with some bindings relative
        // to the action.

        RELVAL *body = ARR_AT(details, IDX_DETAILS_1);

        // The PARAMLIST_HAS_RETURN tricks for definitional return make it
        // seem like a generator authored more code in the action's body...but
        // the code isn't *actually* there and an optimized internal trick is
        // used.  Fake the code if needed.

        REBVAL *example;
        REBLEN real_body_index;
        if (ACT_DISPATCHER(a) == &Voider_Dispatcher) {
            example = Get_System(SYS_STANDARD, STD_PROC_BODY);
            real_body_index = 4;
        }
        else if (ACT_HAS_RETURN(a)) {
            example = Get_System(SYS_STANDARD, STD_FUNC_BODY);
            real_body_index = 4;
        }
        else {
            example = NULL;
            real_body_index = 0; // avoid compiler warning
            UNUSED(real_body_index);
        }

        const REBARR *maybe_fake_body;
        if (example == nullptr) {
            maybe_fake_body = VAL_ARRAY(body);
        }
        else {
            // See %sysobj.r for STANDARD/FUNC-BODY and STANDARD/PROC-BODY
            //
            REBARR *fake = Copy_Array_Shallow_Flags(
                VAL_ARRAY(example),
                VAL_SPECIFIER(example),
                NODE_FLAG_MANAGED
            );

            // Index 5 (or 4 in zero-based C) should be #BODY, a "real" body.
            // To give it the appearance of executing code in place, we use
            // a GROUP!.

            RELVAL *slot = ARR_AT(fake, real_body_index);  // #BODY
            assert(IS_ISSUE(slot));

            // Note: clears VAL_FLAG_LINE
            //
            RESET_VAL_HEADER(slot, REB_GROUP, CELL_FLAG_FIRST_IS_NODE);
            INIT_VAL_NODE(slot, VAL_ARRAY(body));
            VAL_INDEX_RAW(slot) = 0;
            INIT_SPECIFIER(slot, a);  // relative binding

            maybe_fake_body = fake;
        }

        // Cannot give user a relative value back, so make the relative
        // body specific to a fabricated expired frame.  See #2221

        RESET_VAL_HEADER(out, REB_BLOCK, CELL_FLAG_FIRST_IS_NODE);
        INIT_VAL_NODE(out, maybe_fake_body);
        VAL_INDEX_RAW(out) = 0;
        INIT_SPECIFIER(out, Make_Expired_Frame_Ctx_Managed(a));
        return;
    }

    if (ACT_DISPATCHER(a) == &Specializer_Dispatcher) {
        //
        // The FRAME! stored in the body for the specialization has a phase
        // which is actually the function to be run.
        //
        const REBVAL *frame = CTX_ARCHETYPE(ACT_EXEMPLAR(a));
        assert(IS_FRAME(frame));
        Move_Value(out, frame);
        return;
    }

    if (ACT_DISPATCHER(a) == &Generic_Dispatcher) {
        REBVAL *verb = DETAILS_AT(details, 1);
        assert(IS_WORD(verb));
        Move_Value(out, verb);
        return;
    }

    Init_Blank(out); // natives, ffi routines, etc.
    return;
}


//
//  REBTYPE: C
//
// This handler is used to fail for a type which cannot handle actions.
//
// !!! Currently all types have a REBTYPE() handler for either themselves or
// their class.  But having a handler that could be "swapped in" from a
// default failing case is an idea that could be used as an interim step
// to allow something like REB_GOB to fail by default, but have the failing
// type handler swapped out by an extension.
//
REBTYPE(Fail)
{
    UNUSED(frame_);
    UNUSED(verb);

    fail ("Datatype does not have a dispatcher registered.");
}


//
//  Get_If_Word_Or_Path_Throws: C
//
// Originally this routine was used by APPLY and SPECIALIZE type routines
// to allow them to take WORD!s and PATH!s, since there had been no way to
// get the label for an ACTION! after it had been fetched:
//
//     >> applique 'append [value: 'c]  ; APPLIQUE would see the word APPEND
//     ** Script error: append is missing its series argument  ; so name here
//
// That became unnecessary once the mechanics behind VAL_ACTION_LABEL() were
// introduced.  So the interfaces were changed to only take ACTION!, so as
// to be more clear.
//
// This function remains as a utility for other purposes.  It is able to push
// refinements in the process of its fetching, if you want to avoid an
// intermediate specialization when used in apply-like scenarios.
//
bool Get_If_Word_Or_Path_Throws(
    REBVAL *out,
    const RELVAL *v,
    REBSPC *specifier,
    bool push_refinements
) {
    if (IS_WORD(v) or IS_GET_WORD(v) or IS_SYM_WORD(v)) {
      get_as_word:
        Get_Word_May_Fail(out, v, specifier);
        if (IS_ACTION(out))
            INIT_VAL_ACTION_LABEL(out, VAL_WORD_SPELLING(v));
    }
    else if (
        IS_PATH(v) or IS_GET_PATH(v) or IS_SYM_PATH(v)
        or IS_TUPLE(v) or IS_GET_TUPLE(v) or IS_SYM_TUPLE(v)
    ){
        if (ANY_WORD_KIND(HEART_BYTE(v)))  // e.g. `/`
            goto get_as_word;  // faster than calling Eval_Path_Throws_Core?

        if (Eval_Path_Throws_Core(
            out,
            v,  // !!! may not be array based
            specifier,
            nullptr,  // `setval`: null means don't treat as SET-PATH!
            EVAL_MASK_DEFAULT | (push_refinements
                ? EVAL_FLAG_PUSH_PATH_REFINES  // pushed in reverse order
                : 0)
        )){
            return true;
        }
    }
    else
        Derelativize(out, v, specifier);

    return false;
}


//
//  tweak: native [
//
//  {Modify a special property (currently only for ACTION!)}
//
//      return: "Same action identity as input"
//          [action!]
//      action "(modified) Action to modify property of"
//          [action!]
//      property "Currently must be [defer postpone]"
//          [word!]
//      enable [logic!]
//  ]
//
REBNATIVE(tweak)
{
    INCLUDE_PARAMS_OF_TWEAK;

    REBACT *act = VAL_ACTION(ARG(action));
    const REBPAR *first = First_Unspecialized_Param(nullptr, act);

    Reb_Param_Class pclass = first
        ? VAL_PARAM_CLASS(first)
        : REB_P_NORMAL;  // imagine it as <end>able

    REBFLGS flag;

    switch (VAL_WORD_SYM(ARG(property))) {
      case SYM_BARRIER:   // don't allow being taken as an argument, e.g. |
        flag = DETAILS_FLAG_IS_BARRIER;
        break;

      case SYM_DEFER:  // Special enfix behavior used by THEN, ELSE, ALSO...
        if (pclass != REB_P_NORMAL)
            fail ("TWEAK defer only actions with evaluative 1st params");
        flag = DETAILS_FLAG_DEFERS_LOOKBACK;
        break;

      case SYM_POSTPONE:  // Wait as long as it can to run w/o changing order
        if (pclass != REB_P_NORMAL and pclass != REB_P_SOFT)
            fail ("TWEAK postpone only actions with evaluative 1st params");
        flag = DETAILS_FLAG_POSTPONES_ENTIRELY;
        break;

      default:
        fail ("TWEAK currently only supports [barrier defer postpone]");
    }

    if (VAL_LOGIC(ARG(enable)))
        ACT_DETAILS(act)->header.bits |= flag;
    else
        ACT_DETAILS(act)->header.bits &= ~flag;

    RETURN (ARG(action));
}
