//
//  File: %sys-patch.h
//  Summary: "Definitions for Virtual Binding Patches"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2020-2021 Ren-C Open Source Contributors
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
// Virtual Binding patches are small singular arrays which form linked lists
// of contexts.  Patches are in priority order, so that if a word is found in
// the head patch it will resolve there instead of later in the list.
//
// Rather than contain a context, each patch contains a WORD! bound to the
// context it refers to.  The word is the last word in the context at the
// time the patch was created.  This allows a virtual binding to rigorously
// capture the size of the object at the time of its creation--which means
// that a cached property indicating whether a lookup in that patch succeeded
// or not can be trusted.
//
// As an added benefit to using a WORD!, the slot where virtual bind caches
// are stored can be used to cleanly keep a link to the next patch in the
// chain.  Further, there's benefit in that the type of the word can be used
// to indicate if the virtual binding is to all words, just SET-WORD!s, or
// other similar rules.
//
// Whenever possible, one wants to create the same virtual binding chain for
// the same object (or pattern of objects).  Not only does that cut down on
// load for the GC, it also means that it's more likely that a cache lookup
// in a word can be reused.  So the LINK() field of a patch is used to make
// a list of "Variants" of a patch with a different "NextPatch".
//
// Being able to find if there are any existing variants for a context when
// all you have in hand is a context is important.  Rather than make a global
// table mapping contexts to patches, the contexts use their MISC() field
// to link a variant.
//
//=//// NOTES /////////////////////////////////////////////////////////////=//
//
// * Sharing the MISC() field of a context with the meta information is not
//   optimal, as it means the MISC() field of *every* patch has to be given
//   up for a potential meta.  It also means that one patch becomes permanent.
//


// The virtual binding patches keep a circularly linked list of their variants
// that have distinct next pointers.  This way, they can look through that
// list before creating an equivalent chain to one that already exists.
//
#define MISC_Variant_TYPE        REBARR*
#define MISC_Variant_CAST        ARR
#define HAS_MISC_Variant         FLAVOR_PATCH


//=//// PATCH_FLAG_REUSED /////////////////////////////////////////////////=//
//
// It's convenient to be able to know when a patch returned from a make call
// is reused or not.  But adding that parameter to the interface complicates
// it.  There's plenty of bits free on patch array flags, so just use one.
//
// !!! This could use a cell marking flag on the patch's cell, but putting
// it here as a temporary measure.
//
#define PATCH_FLAG_REUSED \
    SERIES_FLAG_24


//=//// PATCH_FLAG_LET ////////////////////////////////////////////////////=//
//
// This signifies that a patch was made using LET, and hence it doesn't point
// to an object...rather the contents are the variable itself.  The LINK()
// holds the symbol.
//
#define PATCH_FLAG_LET \
    SERIES_FLAG_25



// The link slot for patches is available for use...
//
#define LINK_PatchSymbol_TYPE           const REBSYM*
#define LINK_PatchSymbol_CAST           SYM
#define HAS_LINK_PatchSymbol            FLAVOR_PATCH

#define INODE_NextPatch_TYPE            REBARR*
#define INODE_NextPatch_CAST            ARR
#define HAS_INODE_NextPatch             FLAVOR_PATCH

// Next node is either to another patch, a frame specifier REBCTX, or nullptr.
//

#define NextPatch(patch) \
    INODE(NextPatch, patch)


#ifdef NDEBUG
    #define SPC(p) \
        cast(REBSPC*, (p)) // makes UNBOUND look like SPECIFIED

    #define VAL_SPECIFIER(v) \
        SPC(BINDING(v))
#else
    inline static REBSPC* SPC(void *p) {
        assert(p != SPECIFIED); // use SPECIFIED, not SPC(SPECIFIED)

        REBCTX *c = CTX(p);
        assert(CTX_TYPE(c) == REB_FRAME);

        // Note: May be managed or unamanged.

        return cast(REBSPC*, c);
    }

    inline static REBSPC *VAL_SPECIFIER(REBCEL(const*) v) {
        assert(ANY_ARRAY_KIND(CELL_HEART(v)));

        REBARR *a = ARR(BINDING(v));
        if (not a)
            return SPECIFIED;

        if (IS_PATCH(a))
            return cast(REBSPC*, a);  // virtual bind

        // While an ANY-WORD! can be bound specifically to an arbitrary
        // object, an ANY-ARRAY! only becomes bound specifically to frames.
        // The keylist for a frame's context should come from a function's
        // paramlist, which should have an ACTION! value in keylist[0]
        //
        assert(CTX_TYPE(CTX(a)) == REB_FRAME);  // may be inaccessible
        return cast(REBSPC*, a);
    }
#endif


//
// Shared routine that handles linking the patch into the context's variant
// list, and bumping the meta out of the misc into the misc if needed.
//
inline static REBARR *Make_Patch_Core(
    REBCTX *ctx,
    REBLEN limit,
    REBSPC *next,
    enum Reb_Kind kind,
    bool reuse
){
    assert(kind == REB_WORD or kind == REB_SET_WORD);
    assert(limit <= CTX_LEN(ctx));

    // 0 happens with `make object! []` and similar cases.
    //
    // The way virtual binding works, it remembers the length of the context
    // at the time the virtual binding occurred.  This means any keys added
    // after the bind will not be visible.  Hence if the context is empty,
    // this virtual bind can be a no-op.
    //
    // (Note: While it may or may not be desirable to see added variables,
    // allowing that would make it impractical to trust cached virtual bind
    // data that is embedded into words...making caching worthless.  So
    // it is chosen to match the "at that moment" behavior of mutable BIND.)
    //
    if (limit == 0)
        return next;

    // It's possible for a user to try and doubly virtual bind things...but
    // for the moment assume it only happens on accident and alert us to it.
    // Over the long run, this needs to be legal, though.
    //
    if (next and IS_PATCH(next)) {
        assert(BINDING(ARR_SINGLE(next)) != CTX_VARLIST(ctx));
    }

    REBARR *patches = BONUS(Patches, CTX_VARLIST(ctx));
    if (patches) {
        //
        // There's a list of variants in place.  Search it to see if any of
        // them are a match for the given limit and next specifier.
        //
        // !!! Long term this should not search if not reuse.  For now we
        // search just to make sure that you're not putting in a duplicate.
        //
        REBARR *variant = patches;
        do {
            if (
                NextPatch(variant) == next
                and BINDING(ARR_SINGLE(variant)) == CTX_VARLIST(ctx) and
                VAL_WORD_PRIMARY_INDEX_UNCHECKED(ARR_SINGLE(variant)) == limit
            ){
                // The reused flag isn't initially set, but becomes set on
                // the first reuse (and hence every reuse after).  This is
                // useful for the purposes of merging, to know whether to
                // bother searching or not.
                //
                assert(reuse);
                USED(reuse);
                SET_SUBCLASS_FLAG(PATCH, variant, REUSED);
                return variant;
            }
            variant = MISC(Variant, variant);
        } while (variant != patches);

        // We're going to need to make a patch.
    }

    // A virtual bind patch array is a singular node holding an ANY-WORD!
    // bound to the OBJECT! being virtualized against.  The reason for holding
    // the WORD! instead of the OBJECT! in the array cell are:
    //
    // * Gives more header information than storing information already
    //   available in the archetypal context.  So we can assume things like
    //   a SET-WORD! means "only virtual bind the set-words".
    //
    // * Can be used to bind to the last word in the context at the time of
    //   the virtual bind.  This allows for expansion.  The problem with
    //   just using however-many-items-are-current is that it would mean
    //   the extant cached virtual index information could not be trusted.
    //   This gives reproducible effects on when you'll get hits or misses
    //   instead of being subject to the whim of internal cache state.
    //
    // * If something changes the CTX_TYPE() that doesn't have to be reflected
    //   here.  This is a rare case, but happens with MAKE ERROR! in startup
    //   because the standard error object starts life as an object.  (This
    //   mechanism needs revisiting, but it's just another reason.)
    //
    REBARR *patch = Alloc_Singular(
        //
        // LINK is not used yet (likely application: symbol for patches that
        // represent lets).  Consider uses in patches that represent objects.
        // So no SERIES_FLAG_LINK_NODE_NEEDS_MARK yet.
        //
        // MISC is a node, but it's used for linking patches to variants
        // with different chains underneath them...and shouldn't keep that
        // alternate version alive.  So no SERIES_FLAG_MISC_NODE_NEEDS_MARK.
        //
        FLAG_FLAVOR(PATCH)
            | NODE_FLAG_MANAGED
            | SERIES_FLAG_INFO_NODE_NEEDS_MARK
    );

    Init_Any_Word_Bound(ARR_SINGLE(patch), kind, ctx, limit);

    // The way it is designed, the list of patches terminates in either a
    // nullptr or a context pointer that represents the specifying frame for
    // the chain.  So we can simply point to the existing specifier...whether
    // it is a patch, a frame context, or nullptr.
    //
    mutable_INODE(NextPatch, patch) = next;

    // A circularly linked list of variations of this patch with different
    // NextPatch() dta is maintained, to assist in avoiding creating
    // unnecessary duplicates.  Decay_Series() will remove this patch from the
    // list when it is being GC'd.
    //
    if (not patches)
        mutable_MISC(Variant, patch) = patch;
    else {
        mutable_MISC(Variant, patch) = MISC(Variant, patches);
        mutable_MISC(Variant, patches) = patch;
    }

    // Make the last looked for patch the first one that would be found if
    // the same search is used again (assume that's a good strategy)
    //
    mutable_BONUS(Patches, CTX_VARLIST(ctx)) = patch;

    // The LINK field is still available.
    //
    mutable_LINK(PatchSymbol, patch) = nullptr;

    return patch;
}


#define Make_Or_Reuse_Patch(ctx,limit,next,kind) \
    Make_Patch_Core((ctx), (limit), (next), (kind), true)

#define Make_Original_Patch(ctx,limit,next,kind) \
    Make_Patch_Core((ctx), (limit), (next), (kind), false)


//
//  Virtual_Bind_Patchify: C
//
// Update the binding in an array so that it adds the given context as
// overriding the bindings.  This is done without actually mutating the
// structural content of the array...but means words in the array will need
// additional calculations that take the virtual binding chain into account
// as part of Get_Word_Context().
//
// !!! There is a performance tradeoff we could tinker with here, where we
// could build a binder which hashed words to object indices, and then walk
// the block with that binding information to cache in words the virtual
// binding "hits" and "misses".  With small objects this is likely a poor
// tradeoff, as searching them is cheap.  Also it preemptively presumes all
// words would be looked up (many might not be, or might not be intended to
// be looked up with this specifier).  But if the binding chain contains very
// large objects the linear searches might be expensive enough to be worth it.
//
inline static void Virtual_Bind_Patchify(
    REBVAL *any_array,
    REBCTX *ctx,
    enum Reb_Kind kind
){
    // Update array's binding.  Note that once virtually bound, mutating BIND
    // operations might apepar to be ignored if applied to the block.  This
    // makes CONST a good default...and MUTABLE can be used if people are
    // not concerned and want to try binding it through the virtualized
    // reference anyway.
    //
    INIT_BINDING_MAY_MANAGE(
        any_array,
        Make_Or_Reuse_Patch(
            ctx,
            CTX_LEN(ctx),
            VAL_SPECIFIER(any_array),
            kind
        )
    );
    Constify(any_array);
}
