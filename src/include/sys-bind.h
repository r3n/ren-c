//
//  File: %sys-bind.h
//  Summary: "System Binding Include"
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
// R3-Alpha had a per-thread "bind table"; a large and sparsely populated hash
// into which index numbers would be placed, for what index those words would
// have as keys or parameters.  Ren-C's strategy is that binding information
// is wedged into REBSER nodes that represent the canon words themselves.
//
// This would create problems if multiple threads were trying to bind at the
// same time.  While threading was never realized in R3-Alpha, Ren-C doesn't
// want to have any "less of a plan".  So the Reb_Binder is used by binding
// clients as a placeholder for whatever actual state would be used to augment
// the information in the canon word series about which client is making a
// request.  This could be coupled with some kind of lockfree adjustment
// strategy whereby a word that was contentious would cause a structure to
// "pop out" and be pointed to by some atomic thing inside the word.
//
// For the moment, a binder has some influence by saying whether the high 16
// bits or low 16 bits of the canon's misc.index are used.  If the index
// were atomic this would--for instance--allow two clients to bind at once.
// It's just a demonstration of where more general logic using atomics
// that could work for N clients would be.
//
// The debug build also adds another feature, that makes sure the clear count
// matches the set count.
//
// The binding will be either a REBACT (relative to a function) or a
// REBCTX (specific to a context), or simply a plain REBARR such as
// EMPTY_ARRAY which indicates UNBOUND.  The FLAVOR_BYTE() says what it is
//
//     ANY-WORD!: binding is the word's binding
//
//     ANY-ARRAY!: binding is the relativization or specifier for the REBVALs
//     which can be found inside of the frame (for recursive resolution
//     of ANY-WORD!s)
//
//     ACTION!: binding is the instance data for archetypal invocation, so
//     although all the RETURN instances have the same paramlist, it is
//     the binding which is unique to the REBVAL specifying which to exit
//
//     ANY-CONTEXT!: if a FRAME!, the binding carries the instance data from
//     the function it is for.  So if the frame was produced for an instance
//     of RETURN, the keylist only indicates the archetype RETURN.  Putting
//     the binding back together can indicate the instance.
//
//     VARARGS!: the binding identifies the feed from which the values are
//     coming.  It can be an ordinary singular array which was created with
//     MAKE VARARGS! and has its index updated for all shared instances.
//
// Due to the performance-critical nature of these routines, they are inline
// so that locations using them may avoid overhead in invocation.




// Tells whether when an ACTION! has a binding to a context, if that binding
// should override the stored binding inside of a WORD! being looked up.
//
//    o1: make object! [a: 10 f: does [print a]]
//    o2: make o1 [a: 20 b: 22]
//    o3: make o2 [b: 30]
//
// In the scenario above, when calling `f` bound to o2 stored in o2, or the
// call to `f` bound to o3 and stored in o3, the `a` in the relevant objects
// must be found from the override.  This is done by checking to see if a
// walk from the derived keylist makes it down to the keylist for a.
//
// Note that if a new keylist is not made, it's not possible to determine a
// "parent/child" relationship.  There is no information stored which could
// tell that o3 was made from o2 vs. vice-versa.  The only thing that happens
// is at MAKE-time, o3 put its binding into any functions bound to o2 or o1,
// thus getting its overriding behavior.
//
inline static bool Is_Overriding_Context(REBCTX *stored, REBCTX *override)
{
    REBNOD *stored_source = LINK(KeySource, CTX_VARLIST(stored));
    REBNOD *temp = LINK(KeySource, CTX_VARLIST(override));

    // FRAME! "keylists" are actually paramlists, and the LINK.underlying
    // field is used in paramlists (precluding a LINK.ancestor).  Plus, since
    // frames are tied to a function they invoke, they cannot be expanded.
    // For now, deriving from FRAME! is just disabled.
    //
    // Use a faster check for REB_FRAME than CTX_TYPE() == REB_FRAME, since
    // we were extracting keysources anyway. 
    //
    // !!! Note that in virtual binding, something like a FOR-EACH would
    // wind up overriding words bound to FRAME!s, even though not "derived".
    //
    if (Is_Node_Cell(stored_source))
        return false;
    if (Is_Node_Cell(temp))
        return false;

    while (true) {
        if (temp == stored_source)
            return true;

        if (LINK(Ancestor, SER(temp)) == temp)
            break;

        temp = LINK(Ancestor, SER(temp));
    }

    return false;
}


// Modes allowed by Bind related functions:
enum {
    BIND_0 = 0, // Only bind the words found in the context.
    BIND_DEEP = 1 << 1 // Recurse into sub-blocks.
};


struct Reb_Binder {
    bool high;
  #if !defined(NDEBUG)
    REBLEN count;
  #endif

  #if defined(CPLUSPLUS_11)
    //
    // The C++ debug build can help us make sure that no binder ever fails to
    // get an INIT_BINDER() and SHUTDOWN_BINDER() pair called on it, which
    // would leave lingering binding values on REBSER nodes.
    //
    bool initialized;
    Reb_Binder () { initialized = false; }
    ~Reb_Binder () { assert(not initialized); }
  #endif
};


inline static void INIT_BINDER(struct Reb_Binder *binder) {
    binder->high = true; // !!! what about `did (SPORADICALLY(2))` to test?

  #if !defined(NDEBUG)
    binder->count = 0;

    #ifdef CPLUSPLUS_11
        binder->initialized = true;
    #endif
  #endif
}


inline static void SHUTDOWN_BINDER(struct Reb_Binder *binder) {
  #if !defined(NDEBUG)
    assert(binder->count == 0);

    #ifdef CPLUSPLUS_11
        binder->initialized = false;
    #endif
  #endif

    UNUSED(binder);
}


// Tries to set the binder index, but return false if already there.
//
inline static bool Try_Add_Binder_Index(
    struct Reb_Binder *binder,
    const REBSYM *sym,
    REBINT index
){
    REBSTR *s = m_cast(REBSYM*, sym);
    assert(index != 0);
    if (binder->high) {
        if (s->misc.bind_index.high != 0)
            return false;
        s->misc.bind_index.high = index;
    }
    else {
        if (s->misc.bind_index.low != 0)
            return false;
        s->misc.bind_index.low = index;
    }

  #if !defined(NDEBUG)
    ++binder->count;
  #endif
    return true;
}


inline static void Add_Binder_Index(
    struct Reb_Binder *binder,
    const REBSYM *s,
    REBINT index
){
    bool success = Try_Add_Binder_Index(binder, s, index);
    assert(success);
    UNUSED(success);
}


inline static REBINT Get_Binder_Index_Else_0( // 0 if not present
    struct Reb_Binder *binder,
    const REBSYM *s
){
    if (binder->high)
        return s->misc.bind_index.high;
    else
        return s->misc.bind_index.low;
}


inline static REBINT Remove_Binder_Index_Else_0( // return old value if there
    struct Reb_Binder *binder,
    const REBSYM *str
){
    REBSTR *s = m_cast(REBSYM*, str);
    REBINT old_index;
    if (binder->high) {
        old_index = s->misc.bind_index.high;
        if (old_index == 0)
            return 0;
        s->misc.bind_index.high = 0;
    }
    else {
        old_index = s->misc.bind_index.low;
        if (old_index == 0)
            return 0;
        s->misc.bind_index.low = 0;
    }

  #if !defined(NDEBUG)
    assert(binder->count > 0);
    --binder->count;
  #endif
    return old_index;
}


inline static void Remove_Binder_Index(
    struct Reb_Binder *binder,
    const REBSYM *s
){
    REBINT old_index = Remove_Binder_Index_Else_0(binder, s);
    assert(old_index != 0);
    UNUSED(old_index);
}


// Modes allowed by Collect keys functions:
enum {
    COLLECT_ONLY_SET_WORDS = 0,
    COLLECT_ANY_WORD = 1 << 1,
    COLLECT_DEEP = 1 << 2,
    COLLECT_NO_DUP = 1 << 3  // Do not allow dups during collection (for specs)
};

struct Reb_Collector {
    REBFLGS flags;
    REBDSP dsp_orig;
    struct Reb_Binder binder;
};

#define Collector_Index_If_Pushed(collector) \
    ((DSP - (collector)->dsp_orig) + 1)  // index of *next* item to add


// The process of derelativization will resolve a relative value with a
// specific one--storing frame references into cells.  But once that has
// happened, the cell may outlive the frame...but the binding override that
// the frame contributed might still matter.
//
// !!! The functioning of Decay_Series() should be reviewed to see if it
// actually needs to preserve the CTX_ARCHETYPE().  It's not entirely clear
// if the scenarios are meaningful--but Derelativize cannot fail(), and
// it would without this.  It might also put in some "fake" element that
// would fail later, but given that the REBFRM's captured binding can outlive
// the frame that might lose important functionality.
//
inline static REBSER *SPC_BINDING(REBSPC *specifier)
{
    assert(specifier != UNBOUND);
    const REBVAL *rootvar = CTX_ARCHETYPE(CTX(specifier));  // ok if Decay()'d
    assert(IS_FRAME(rootvar));
    return BINDING(rootvar);
}


// If the cell we're writing into is a stack cell, there's a chance that
// management/reification of the binding can be avoided.
//
// Payload and header should be valid prior to making this call.
//
inline static void INIT_BINDING_MAY_MANAGE(
    RELVAL *out,
    const REBSER* binding
){
    mutable_BINDING(out) = binding;

    if (not binding or GET_SERIES_FLAG(binding, MANAGED))
        return;  // unbound or managed already (frame OR object context)

    REBFRM *f = FRM(LINK(KeySource, binding));  // unmanaged only frame
    assert(f->key == f->key_tail);  // cannot manage varlist in mid fulfill!
    UNUSED(f);

    m_cast(REBSER*, binding)->leader.bits |= NODE_FLAG_MANAGED;  // GC sees...
}


// The unbound state for an ANY-WORD! is to hold its spelling.  Once bound,
// the spelling is derived by indexing into the keylist of the binding (if
// bound directly to a context) or into the paramlist (if relative to an
// action, requiring a frame specifier to fully resolve).
//
inline static bool IS_WORD_UNBOUND(const RELVAL *v) {
    assert(ANY_WORD_KIND(CELL_HEART(VAL_UNESCAPED(v))));
    return IS_SYMBOL(BINDING(v));
}

#define IS_WORD_BOUND(v) \
    (not IS_WORD_UNBOUND(v))


inline static REBLEN VAL_WORD_INDEX(const RELVAL *v) {
    assert(IS_WORD_BOUND(v));
    uint32_t i = VAL_WORD_PRIMARY_INDEX_UNCHECKED(v);
    assert(i > 0);
    return cast(REBLEN, i);
}

inline static REBARR *VAL_WORD_BINDING(const RELVAL *v) {
    assert(ANY_WORD_KIND(CELL_HEART(VAL_UNESCAPED(v))));
    REBSER *binding = BINDING(v);  // Note: is const if REBSTR...
    if (IS_SYMBOL(binding))
        return UNBOUND;
    return ARR(binding);
}

inline static void INIT_VAL_WORD_BINDING(RELVAL *v, const REBSER *binding) {
    assert(ANY_WORD_KIND(CELL_HEART(VAL_UNESCAPED(v))));

    assert(binding);  // can't set word bindings to nullptr
    mutable_BINDING(v) = binding;

  #if !defined(NDEBUG)
    if (IS_SYMBOL(binding))
        return;  // e.g. UNBOUND (words use strings to indicate unbounds)

    if (binding->leader.bits & NODE_FLAG_MANAGED) {
        assert(
            IS_DETAILS(binding)  // relative
            or IS_VARLIST(binding)  // specific
            or IS_PATCH(binding)  // let
        );
    }
    else
        assert(IS_VARLIST(binding));
  #endif
}


// While ideally error messages would give back data that is bound exactly to
// the context that was applicable, threading the specifier into many cases
// can overcomplicate code.  We'd break too many invariants to just say a
// relativized value is "unbound", so make an expired frame if necessary.
//
inline static REBVAL* Unrelativize(RELVAL* out, const RELVAL* v) {
    if (not Is_Bindable(v) or IS_SPECIFIC(v))
        Copy_Cell(out, SPECIFIC(v));
    else {  // must be bound to a function
        REBACT *binding = ACT(VAL_WORD_BINDING(v));
        REBCTX *expired = Make_Expired_Frame_Ctx_Managed(binding);

        Copy_Cell_Header(out, v);
        out->payload = v->payload;
        mutable_BINDING(out) = expired;
    }
    return cast(REBVAL*, out);
}

// This is a super lazy version of unrelativization, which can be used to
// hand a relative value to something like fail(), since fail will clean up
// the stray alloc.
//
#define rebUnrelativize(v) \
    Unrelativize(Alloc_Value(), (v))

inline static void Unbind_Any_Word(RELVAL *v) {
    const REBSTR *spelling = VAL_WORD_SYMBOL(VAL_UNESCAPED(v));
    INIT_VAL_WORD_BINDING(v, spelling);
    INIT_VAL_WORD_PRIMARY_INDEX(v, 0);
}

inline static REBCTX *VAL_WORD_CONTEXT(const REBVAL *v) {
    assert(IS_WORD_BOUND(v));
    REBARR *binding = VAL_WORD_BINDING(v);
    assert(
        GET_SERIES_FLAG(binding, MANAGED) or
        FRM(LINK(KeySource, binding))->key
            == FRM(LINK(KeySource, binding))->key_tail  // not fulfilling
    );
    binding->leader.bits |= NODE_FLAG_MANAGED;  // !!! review managing needs
    REBCTX *c = CTX(binding);
    FAIL_IF_INACCESSIBLE_CTX(c);
    return c;
}

// When a word is bound, its spelling is derived from the context it is bound
// to.  This means getting at the spelling will cost slightly more, but frees
// up space in word cell for other features.  Note that this means if a
// context is freed, its keylist must be retained to provide the words.
//
inline static const REBSYM *VAL_WORD_SYMBOL(REBCEL(const*) cell) {
    assert(ANY_WORD_KIND(CELL_HEART(cell)));

    if (IS_SYMBOL(BINDING(cell)))
        return SYM(BINDING(cell));

    REBARR *binding = ARR(BINDING(cell));

    // Note: inside QUOTED! cells, all words should be bound to strings.  This
    // is because different bindings can be made at each reference site.
    // So at this point, we can be certain the cell is a RELVAL.

    const RELVAL *v = CELL_TO_VAL(cell);

    if (IS_DETAILS(binding))  // relative
        return KEY_SYMBOL(ACT_KEY(ACT(binding), VAL_WORD_INDEX(v)));

    if (IS_PATCH(binding))  // let
        return LINK(PatchSymbol, binding);

    assert(IS_VARLIST(binding));  // specific
    return KEY_SYMBOL(CTX_KEY(CTX(binding), VAL_WORD_INDEX(v)));
}


//=////////////////////////////////////////////////////////////////////////=//
//
//  VARIABLE ACCESS
//
//=////////////////////////////////////////////////////////////////////////=//
//
// When a word is bound to a context by an index, it becomes a means of
// reading and writing from a persistent storage location.  We use "variable"
// or just VAR to refer to REBVAL slots reached via binding in this way.
// More narrowly, a VAR that represents an argument to a function invocation
// may be called an ARG (and an ARG's "persistence" is only as long as that
// function call is on the stack).
//
// All variables can be put in a CELL_FLAG_PROTECTED state.  This is a flag
// on the variable cell itself--not the key--so different instances of
// the same object sharing the keylist don't all have to be protected just
// because one instance is.  This is not one of the flags included in the
// CELL_MASK_COPIED, so it shouldn't be able to leak out of the varlist.
//
// The Lookup_Word_May_Fail() function takes the conservative default that
// only const access is needed.  A const pointer to a REBVAL is given back
// which may be inspected, but the contents not modified.  While a bound
// variable that is not currently set will return a REB_NULL value,
// Lookup_Word_May_Fail() on an *unbound* word will raise an error.
//
// Lookup_Mutable_Word_May_Fail() offers a parallel facility for getting a
// non-const REBVAL back.  It will fail if the variable is either unbound
// -or- marked with OPT_TYPESET_LOCKED to protect against modification.
//


// Find the context a word is bound into.  This must account for the various
// binding forms: Relative Binding, Derived Binding, and Virtual Binding.
//
// The reason this is broken out from the Lookup_Word() routines is because
// sometimes read-only-ness of the context is heeded, and sometimes it is not.
// Splitting into a step that returns the context and the index means the
// main work of finding where to look up doesn't need to be parameterized
// with that.
//
// This function is used by Derelativize(), and so it shouldn't have any
// failure mode while it's running...even if the context is inaccessible or
// the word is unbound.  Errors should be raised by callers if applicable.
//
inline static option(REBARR*) Get_Word_Container(
    REBLEN *index_out,
    const RELVAL* any_word,
    REBSPC *specifier
){
  #if !defined(NDEBUG)
    *index_out = 0xDECAFBAD;  // trash index to make sure it gets set
  #endif

    REBARR *binding = VAL_WORD_BINDING(any_word);

    if (specifier == SPECIFIED) {  // Note: may become SPECIFIED again below
        if (binding == UNBOUND)
            return nullptr;

        assert(IS_VARLIST(binding) or IS_PATCH(binding));  // not relative
        *index_out = VAL_WORD_INDEX(any_word);
        return binding;
    }

    // Virtual binding shortcut; if a virtual binding is in effect and it
    // matches the cache in the word, then trust the information in it...
    // whether that's a hit or a miss.
    //
    if (specifier == VAL_WORD_CACHE(any_word)) {
        //
        // Since the number of bits available in a virtual bind is limited,
        // the value stored is the index modulo MONDEX_MOD.  A miss is
        // recorded with the actual value MONDEX_MOD (since 0 can be an
        // actual modulus result).
        //
        REBLEN mondex = VAL_WORD_VIRTUAL_MONDEX_UNCHECKED(any_word);
        if (mondex == MONDEX_MOD)
            goto virtual_miss;

      blockscope {
        const REBSTR *spelling = VAL_WORD_SYMBOL(VAL_UNESCAPED(any_word));

        // We have the primary binding's spelling to check against, so we
        // can recognize when the lossy index matches up.  It needs to match
        // one of the virtual overriding contexts...we don't have enough bits
        // to say which one so check them all.
        //
        // !!! To improve locality it might be better to take a couple of
        // mondex bits to use as the mod of the chain length.
        //
        do {
            assert(IS_PATCH(specifier));

            if (GET_SUBCLASS_FLAG(PATCH, specifier, LET)) {
                if (LINK(PatchSymbol, specifier) == spelling) {
                    *index_out = 1;  // !!! lie, review
                    return specifier;
                }
                goto skip_hit_patch;
            }

            if (
                IS_SET_WORD(ARR_SINGLE(specifier))
                and REB_SET_WORD != CELL_KIND(VAL_UNESCAPED(any_word))
            ){
                goto skip_hit_patch;
            }

          blockscope {
            REBCTX *overload = CTX(BINDING(ARR_SINGLE(specifier)));

            // Length at time of virtual bind is cached by index.  This avoids
            // allowing untrustworthy cache states.
            //
            REBLEN cached_len = VAL_WORD_INDEX(ARR_SINGLE(specifier));

            REBLEN index = mondex;
            for (; index <= cached_len; index += MONDEX_MOD) {
                if (spelling != KEY_SYMBOL(CTX_KEY(overload, mondex)))
                    continue;

                *index_out = mondex;
                return CTX_VARLIST(overload);
            }
          }

          skip_hit_patch:
            specifier = NextPatch(specifier);
        } while (
            specifier and not IS_VARLIST(specifier)
        );

        panic (any_word);  // bad cache in value
      }
    }

  virtual_miss:

    if (IS_PATCH(specifier)) {
        //
        // Bad news: We have a virtual bind in effect, but not the virtual
        // bind that is cached in the word.  We have no way of knowing if
        // this word is overridden without doing a linear search.  Do it
        // and then save the hit or miss information in the word for next use.
        //
        INIT_VAL_WORD_CACHE(any_word, specifier);  // we're updating it

        const REBSTR *spelling = VAL_WORD_SYMBOL(VAL_UNESCAPED(any_word));

        // !!! Virtual binding could use the bind table as a kind of next
        // level cache if it encounters a large enough object to make it
        // wortwhile?
        //
        do {
            if (GET_SUBCLASS_FLAG(PATCH, specifier, LET)) {
                if (LINK(PatchSymbol, specifier) == spelling) {
                    *index_out = 1;  // !!! lie, review
                    return specifier;
                }
                goto skip_miss_patch;
            }

            if (
                IS_SET_WORD(ARR_SINGLE(specifier))
                and REB_SET_WORD != CELL_KIND(VAL_UNESCAPED(any_word))
            ){
                goto skip_miss_patch;
            }

          blockscope {
            REBCTX *overload = CTX(BINDING(ARR_SINGLE(specifier)));

            // Length at time of virtual bind is cached by index.  This avoids
            // allowing untrustworthy cache states.
            //
            REBLEN cached_len = VAL_WORD_INDEX(ARR_SINGLE(specifier));

            REBLEN index = 1;
            const REBKEY *key = CTX_KEYS_HEAD(overload);
            for (; index <= cached_len; ++key, ++index) {
                if (KEY_SYMBOL(key) != spelling)
                    continue;

                // !!! FOR-EACH uses the slots in an object to count how
                // many arguments there are...and if a slot is reusing an
                // existing variable it holds that variable.  This ties into
                // general questions of hiding which is the same bit.  Don't
                // count it as a hit.
                //
                if (GET_CELL_FLAG(CTX_VAR(overload, index), BIND_NOTE_REUSE))
                    break;

                // Found a match!  Cache it to speed up next time.  Note that
                // since specifier chains change frames for relativization,
                // we have to store the head of the chain.  Review.
                //
                INIT_VAL_WORD_VIRTUAL_MONDEX(any_word, index % MONDEX_MOD);
                *index_out = index;
                return CTX_VARLIST(overload);
            }
          }
          skip_miss_patch:
            specifier = NextPatch(specifier);
        } while (
            specifier and not IS_VARLIST(specifier)
        );

        // Update the cache to say we miss on this particular specifier
        //
        INIT_VAL_WORD_VIRTUAL_MONDEX(any_word, MONDEX_MOD);

        // The linked list of specifiers bottoms out with either null or the
        // varlist of the frame we want to bind relative values with.  So
        // `specifier` should be set now.
    }

    assert(specifier == SPECIFIED or IS_VARLIST(specifier));

    REBCTX *c;

    if (binding == UNBOUND)
        return nullptr;  // once no virtual bind found, no binding is unbound

    if (IS_PATCH(binding)) {
        //
        // LET BINDING: Directly bound to a LET variable.  This happens when
        // a word that is bound to a LET gets copied so it's not virtual.
        //
        assert(GET_SUBCLASS_FLAG(PATCH, binding, LET));
        *index_out = 1;  // !!! lie, review
        return binding;
    }

    if (IS_VARLIST(binding)) {

        // SPECIFIC BINDING: The context the word is bound to is explicitly
        // contained in the `any_word` REBVAL payload.  Extract it, but check
        // to see if there is an override via "DERIVED BINDING", e.g.:
        //
        //    o1: make object [a: 10 f: meth [] [print a]]
        //    o2: make o1 [a: 20]
        //
        // O2 doesn't copy F's body, but its copy of the ACTION! cell in o2/f
        // gets its ->binding to point at O2 instead of O1.  When o2/f runs,
        // the frame stores that pointer, and we take it into account when
        // looking up `a` here, instead of using a's stored binding directly.

        c = CTX(binding); // start with stored binding

        if (specifier == SPECIFIED) {
            //
            // Lookup must be determined solely from bits in the value
            //
        }
        else {
            REBSER *f_binding = SPC_BINDING(specifier); // can't fail()
            if (f_binding and Is_Overriding_Context(c, CTX(f_binding))) {
                //
                // The specifier binding overrides--because what's happening 
                // is that this cell came from a METHOD's body, where the
                // particular ACTION! value cell triggering it held a binding
                // of a more derived version of the object to which the
                // instance in the method body refers.
                //
                c = CTX(f_binding);
            }
        }
    }
    else {
        assert(IS_DETAILS(binding));

        // RELATIVE BINDING: The word was made during a deep copy of the block
        // that was given as a function's body, and stored a reference to that
        // ACTION! as its binding.  To get a variable for the word, we must
        // find the right function call on the stack (if any) for the word to
        // refer to (the FRAME!)

      #if !defined(NDEBUG)
        if (specifier == SPECIFIED) {
            printf("Get_Context_Core on relative value without specifier\n");
            panic (any_word);
        }
      #endif

        c = CTX(specifier);

        // We can only check for a match of the underlying function.  If we
        // checked for an exact match, then the same function body could not
        // be repurposed for dispatch e.g. in copied, hijacked, or adapted
        // code, because the identity of the derived function would not match
        // up with the body it intended to reuse.
        //
        assert(Action_Is_Base_Of(ACT(binding), CTX_FRAME_ACTION(c)));
    }

    *index_out = VAL_WORD_INDEX(any_word);
    return CTX_VARLIST(c);
}

static inline const REBVAL *Lookup_Word_May_Fail(
    const RELVAL *any_word,
    REBSPC *specifier
){
    REBLEN index;
    REBARR *a = try_unwrap(Get_Word_Container(&index, any_word, specifier));
    if (not a)
        fail (Error_Not_Bound_Raw(SPECIFIC(any_word)));
    if (IS_PATCH(a))
        return SPECIFIC(ARR_SINGLE(a));
    REBCTX *c = CTX(a);
    if (GET_SERIES_FLAG(CTX_VARLIST(c), INACCESSIBLE))
        fail (Error_No_Relative_Core(any_word));

    return CTX_VAR(c, index);
}

static inline option(const REBVAL*) Lookup_Word(
    const RELVAL *any_word,
    REBSPC *specifier
){
    REBLEN index;
    REBARR *a = try_unwrap(Get_Word_Container(&index, any_word, specifier));
    if (not a)
        return nullptr;
    if (IS_PATCH(a))
        return SPECIFIC(ARR_SINGLE(a));
    REBCTX *c = CTX(a);
    if (GET_SERIES_FLAG(CTX_VARLIST(c), INACCESSIBLE))
        return nullptr;

    return CTX_VAR(c, index);
}

static inline const REBVAL *Get_Word_May_Fail(
    RELVAL *out,
    const RELVAL* any_word,
    REBSPC *specifier
){
    const REBVAL *var = Lookup_Word_May_Fail(any_word, specifier);
    if (IS_VOID(var))
        fail (Error_Need_Non_Void_Core(
            cast(const REBVAL*, any_word), specifier,
            var
        ));

    return Copy_Cell(out, var);
}

static inline REBVAL *Lookup_Mutable_Word_May_Fail(
    const RELVAL* any_word,
    REBSPC *specifier
){
    REBLEN index;
    REBARR *a = try_unwrap(Get_Word_Container(&index, any_word, specifier));
    if (not a)
        fail (Error_Not_Bound_Raw(SPECIFIC(any_word)));

    REBVAL *var;
    if (IS_PATCH(a))
        var = SPECIFIC(ARR_SINGLE(a));
    else {
        REBCTX *c = CTX(a);

        // A context can be permanently frozen (`lock obj`) or temporarily
        // protected, e.g. `protect obj | unprotect obj`.  A native will
        // use SERIES_FLAG_HOLD on a FRAME! context in order to prevent
        // setting values to types with bit patterns the C might crash on.
        //
        // Lock bits are all in SER->info and checked in the same instruction.
        //
        FAIL_IF_READ_ONLY_SER(CTX_VARLIST(c));

        var = CTX_VAR(c, index);
    }

    // The PROTECT command has a finer-grained granularity for marking
    // not just contexts, but individual fields as protected.
    //
    if (GET_CELL_FLAG(var, PROTECTED)) {
        DECLARE_LOCAL (unwritable);
        Init_Word(unwritable, VAL_WORD_SYMBOL(any_word));
        fail (Error_Protected_Word_Raw(unwritable));
    }

    return var;
}

inline static REBVAL *Sink_Word_May_Fail(
    const RELVAL* any_word,
    REBSPC *specifier
){
    REBVAL *var = Lookup_Mutable_Word_May_Fail(any_word, specifier);
    TRASH_CELL_IF_DEBUG(var);
    return var;
}


//=////////////////////////////////////////////////////////////////////////=//
//
//  COPYING RELATIVE VALUES TO SPECIFIC
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This can be used to turn a RELVAL into a REBVAL.  If the RELVAL is indeed
// relative and needs to be made specific to be put into the target, then the
// specifier is used to do that.
//
// It is nearly as fast as just assigning the value directly in the release
// build, though debug builds assert that the function in the specifier
// indeed matches the target in the relative value (because relative values
// in an array may only be relative to the function that deep copied them, and
// that is the only kind of specifier you can use with them).
//
// Interface designed to line up with Copy_Cell()
//
// !!! At the moment, there is a fair amount of overlap in this code with
// Get_Context_Core().  One of them resolves a value's real binding and then
// fetches it, while the other resolves a value's real binding but then stores
// that back into another value without fetching it.  This suggests sharing
// a mechanic between both...TBD.
//

inline static REBSPC *Derive_Specifier(
    REBSPC *parent,
    const RELVAL* any_array
);

#ifdef CPLUSPLUS_11
    inline static REBSPC *Derive_Specifier(
        REBSPC *parent,
        const REBVAL* any_array
    ) = delete;
#endif

inline static REBVAL *Derelativize(
    RELVAL *out,  // relative dest overwritten w/specific value
    const RELVAL *v,
    REBSPC *specifier
){
    Copy_Cell_Header(out, v);
    out->payload = v->payload;
    if (not Is_Bindable(v)) {
        out->extra = v->extra;
        return cast(REBVAL*, out);
    }

    enum Reb_Kind heart = CELL_HEART(VAL_UNESCAPED(v));

    // For words, we go ahead and pay for the lookup at the moment of a
    // derelativize.  While this is a bit unfortunate to have to pay the cost
    // even if a WORD!s binding is not going to be used, it helps reduce the
    // spread of patch specifier nodes in the system.
    //
    if (ANY_WORD_KIND(heart)) {
        REBLEN index;
        REBARR *a = try_unwrap(Get_Word_Container(&index, v, specifier));
        if (not a) {
            assert(VAL_WORD_BINDING(v) == UNBOUND);
            out->extra = v->extra;
            Unbind_Any_Word(out);  // !!! do this more efficiently
        }
        else {
            out->extra = v->extra;  // !!! to know spelling in binding, temp
            INIT_BINDING_MAY_MANAGE(out, a);
            INIT_VAL_WORD_PRIMARY_INDEX(out, index);
        }

        // When we resolve a word specifically, we clear out the specifier
        // cache.  The same virtual specifier is unlikely to be used with it
        // again (as any new series are pulled out of the "wave" of binding).
        //
        // We don't want to do this with REB_QUOTED since the cache is shared.
        //
        if (KIND3Q_BYTE_UNCHECKED(v) != REB_QUOTED) {
            INIT_VAL_WORD_CACHE(out, UNSPECIFIED);
            INIT_VAL_WORD_VIRTUAL_MONDEX(out, MONDEX_MOD);  // necessary?
        }
        return cast(REBVAL*, out);
    }
    else if (ANY_ARRAY_KIND(heart)) {
        //
        // The job of an array in a derelativize operation is to carry along
        // the specifier.  However, it cannot lose any prior existing info
        // that's in the specifier it holds.
        //
        // THE BINDING IN ARRAYS MAY BE UNMANAGED...due to an optimization
        // for passing things to natives that is probably not needed any
        // longer.  Review.
        //
        // The mechanism otherwise is shared with specifier derivation.
        // That includes the case of if specifier==SPECIFIED.
        //
        INIT_BINDING_MAY_MANAGE(out, Derive_Specifier(specifier, v));
    }
    else {
        // Things like contexts and varargs are not affected by specifiers,
        // at least not currently.
        //
        out->extra = v->extra;
    }

    return cast(REBVAL*, out);
}


// In the C++ build, defining this overload that takes a REBVAL* instead of
// a RELVAL*, and then not defining it...will tell you that you do not need
// to use Derelativize.  Juse Copy_Cell() if your source is a REBVAL!
//
#ifdef CPLUSPLUS_11
    REBVAL *Derelativize(RELVAL *dest, const REBVAL *v, REBSPC *specifier);
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  DETERMINING SPECIFIER FOR CHILDREN IN AN ARRAY
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A relative array must be combined with a specifier in order to find the
// actual context instance where its values can be found.  Since today's
// specifiers are always nothing or a FRAME!'s context, this is fairly easy...
// if you find a specific child value living inside a relative array then
// it's that child's specifier that overrides the specifier in effect.
//
// With virtual binding this could get more complex, since a specifier may
// wish to augment or override the binding in a deep way on read-only blocks.
// That means specifiers may need to be chained together.  This would create
// needs for GC or reference counting mechanics, which may defy a simple
// solution in C89.
//
// But as a first step, this function locates all the places in the code that
// would need such derivation.
//

// A specifier can be a FRAME! context for fulfilling relative words.  Or it
// may be a chain of virtual binds where the last link in the chain is to
// a frame context.
//
// It's Derive_Specifier()'s job to make sure that if specifiers get linked on
// top of each other, the chain always bottoms out on the same FRAME! that
// the original specifier was pointing to.
//
inline static REBNOD** SPC_FRAME_CTX_ADDRESS(REBSPC *specifier)
{
    assert(IS_PATCH(specifier));
    while (
        NextPatch(specifier) != nullptr
        and not IS_VARLIST(NextPatch(specifier))
    ){
        specifier = NextPatch(specifier);
    }
    return &node_INODE(NextPatch, specifier);
}

inline static option(REBCTX*) SPC_FRAME_CTX(REBSPC *specifier)
{
    if (specifier == UNBOUND)  // !!! have caller check?
        return nullptr;
    if (IS_VARLIST(specifier))
        return CTX(specifier);
    return CTX(*SPC_FRAME_CTX_ADDRESS(specifier));
}


// This routine will merge virtual binding patches, returning one where the
// child is at the beginning of the chain.  This will preserve the child's
// frame resolving context (if any) that terminates it.
//
// If the returned chain manages to reuse an existing case, then the result
// will have ARRAY_FLAG_PATCH_REUSED set.  This can inform higher levels of
// whether it's worth searching their patchlist or not...as newly created
// patches can't appear in their prior create list.
//
inline static REBARR *Merge_Patches_May_Reuse(
    REBARR *parent,
    REBARR *child
){
    assert(IS_PATCH(parent));
    assert(IS_PATCH(child));

    // If we find the child already accounted for in the parent, we're done.
    // Recursions should notice this case and return up to make a no-op.
    //
    if (NextPatch(parent) == child) {
        SET_SUBCLASS_FLAG(PATCH, parent, REUSED);
        return parent;  // reused existing
    }

    // If we get to the end of the merge chain and don't find the child, then
    // we're going to need a patch that incorporates it.
    //
    REBARR *next;
    if (NextPatch(parent) == nullptr or IS_VARLIST(NextPatch(parent))) {
        next = child;
        SET_SUBCLASS_FLAG(PATCH, next, REUSED);
    }
    else
        next = Merge_Patches_May_Reuse(NextPatch(parent), child);

    return Make_Patch_Core(
        CTX(BINDING(ARR_SINGLE(parent))),
        VAL_WORD_INDEX(ARR_SINGLE(parent)),
        next,
        VAL_TYPE(ARR_SINGLE(parent)),
        GET_SUBCLASS_FLAG(PATCH, next, REUSED)
    );
}


// An ANY-ARRAY! cell has a pointer's-worth of spare space in it, which is
// used to keep track of the information required to further resolve the
// words and arrays that are inside of it.  Each time code wishes to take a
// step descending into an array's contents, this "specifier" information
// must be merged with the specifier that is being applied.
//
// Specifier state only accrues in this way while descending through nodes.
// Jumping to a new value...e.g. fetching a REBVAL* out of a WORD! variable,
// should restart the process with a new specifier.
//
// The returned specifier must not lose the ability to resolve relative
// values, so it has to remember what frame relative values are for.
//
inline static REBSPC *Derive_Specifier_Core(
    REBSPC *specifier,  // merge this specifier...
    const RELVAL* any_array  // ...onto the one in this array
){
    REBARR *old = ARR(BINDING(any_array));

    if (specifier == SPECIFIED) {  // no override being requested
        assert(old == UNBOUND or IS_VARLIST(old) or IS_PATCH(old));
        return old;  // so give back what was the array was holding
    }

    if (old == UNBOUND) {  // no binding information in the incoming cell
        //
        // It is legal to use a specifier with a "fully resolved" value.
        // A virtual specifier must be propagated, but it's not necessary to
        // add a frame node.  While it would be "harmless" to put it on, it
        // would mean specifier chains would have to be made to preserve it
        // when it wasn't actually useful...and it taxes the GC.  Drop if
        // possible.
        //
        if (not IS_PATCH(specifier))
            return SPECIFIED;

        return specifier;  // so just propagate the incoming specifier
    }

    // v-- NOTE: The following two IFs just `return specifier`.  Separate for
    // clarity and assertions, but trust the optimizer to fold them together
    // in the release build.

    if (specifier == old) {  // a no-op, specifier was already applied
        assert(IS_VARLIST(specifier) or IS_PATCH(specifier));
        return specifier;
    }

    if (IS_DETAILS(old)) {
        //
        // The stored binding is relative to a function, and so the specifier
        // we have *must* be able to give us the matching FRAME! instance.
        //
        // We have to be content with checking for a match in underlying
        // functions, vs. checking for an exact match.  Else hijackings or
        // COPY'd actions, or adapted preludes, could not match up with
        // actions put in the specifier.  We'd have to make new and
        // re-relativized copies of the bodies--which is not only wasteful,
        // but breaks the "black box" quality of function composition.
        //
      #if !defined(NDEBUG)
        REBCTX *frame_ctx = try_unwrap(SPC_FRAME_CTX(specifier));
        if (
            frame_ctx == nullptr
            or (
                NOT_SERIES_FLAG(CTX_VARLIST(frame_ctx), INACCESSIBLE) and
                not Action_Is_Base_Of(ACT(old), CTX_FRAME_ACTION(frame_ctx))
            )
        ){
            printf("Function mismatch in specific binding, expected:\n");
            PROBE(ACT_ARCHETYPE(ACT(old)));
            printf("Panic on relative value\n");
            panic (any_array);
        }
      #endif

        return specifier;  // input specifier will serve for derelativizations
    }

    // Either binding or the specifier have virtual components.  Whatever
    // happens, the specifier we give back has to have the frame resolution
    // compatible with what's in the value.

    if (IS_VARLIST(old)) {
        //
        // If the array cell is already holding a frame, then it intends to
        // broadcast that down for resolving relative values underneath it.
        // We can only pass thru the incoming specifier if it is compatible.
        // Otherwise we need a new specifier that folds in the binding.
        //
        assert(IS_PATCH(specifier));

        // !!! This case of a match could be handled by the swap below, but
        // break it out separately for now for the sake of asserts.
        //
        // !!! We already know it's a patch so calling SPC_FRAME_CTX() does
        // an extra check of that, review when efficiency is being revisited
        // (SPC_PATCH_CTX() as separate entry point?)
        //
        REBNOD **specifier_frame_ctx_addr = SPC_FRAME_CTX_ADDRESS(specifier);
        if (*specifier_frame_ctx_addr == old)  // all clear to reuse
            return specifier;

        if (*specifier_frame_ctx_addr == UNSPECIFIED) {
            //
            // If the patch had no specifier, then it doesn't hurt to modify
            // it directly.  This will only work once for specifier's chain.
            //
            *specifier_frame_ctx_addr = old;
            return specifier;
        }

        // Patch resolves to a binding, and it's an incompatible one.  If
        // this happens, we have to copy the whole chain.  Is this possible?
        // Haven't come up with a situation that forces it yet.

        panic ("Incompatible patch bindings; if you hit this, report it.");
    }

    // The situation for if the array is already holding a patch is that we
    // have to integrate our new patch on top of it.
    //
    // !!! How do we make sure this doesn't make a circularly linked list?

    assert(IS_PATCH(old));

    if (not IS_PATCH(specifier)) {
        assert(IS_VARLIST(specifier));
        return old;  // The binding can be disregarded on this value
    }

    // The patch might be able to be reused and it might not, so it may carry
    // the PATCH_REUSED array flag.  Is that interesting information here?
    //
    return Merge_Patches_May_Reuse(specifier, old);
}

#if !defined(NDEBUG)
    #define DEBUG_VIRTUAL_BINDING
#endif
#if !defined(DEBUG_VIRTUAL_BINDING)
    inline static REBSPC *Derive_Specifier(
        REBSPC *specifier,
        const RELVAL* any_array
    ){
        return Derive_Specifier_Core(specifier, any_array);
    }
#else
    inline static REBSPC *Derive_Specifier(
        REBSPC *specifier,
        const RELVAL* any_array
    ){
        REBSPC *derived = Derive_Specifier_Core(specifier, any_array);
        REBARR *old = ARR(BINDING(any_array));
        if (old == UNSPECIFIED or IS_VARLIST(old)) {
            // no special invariant to check, anything goes for derived
        }
        else if (IS_DETAILS(old)) {  // relative
            REBCTX *derived_ctx = try_unwrap(SPC_FRAME_CTX(derived));
            REBCTX *specifier_ctx = try_unwrap(SPC_FRAME_CTX(specifier));
            assert(derived_ctx == specifier_ctx);
        }
        else {
            assert(IS_PATCH(old));

            REBCTX *binding_ctx = try_unwrap(SPC_FRAME_CTX(old));
            if (binding_ctx == UNSPECIFIED) {
                // anything goes for the frame in the derived specifier
            }
            else {
                REBCTX *derived_ctx = try_unwrap(SPC_FRAME_CTX(derived));
                assert(derived_ctx == binding_ctx);
            }
        }
        return derived;
    }
#endif


//
// BINDING CONVENIENCE MACROS
//
// WARNING: Don't pass these routines something like a singular REBVAL* (such
// as a REB_BLOCK) which you wish to have bound.  You must pass its *contents*
// as an array...as the plural "values" in the name implies!
//
// So don't do this:
//
//     REBVAL *block = ARG(block);
//     REBVAL *something = ARG(next_arg_after_block);
//     Bind_Values_Deep(block, context);
//
// What will happen is that the block will be treated as an array of values
// and get incremented.  In the above case it would reach to the next argument
// and bind it too (likely crashing at some point not too long after that).
//
// Instead write:
//
//     Bind_Values_Deep(ARR_HEAD(VAL_ARRAY(block)), context);
//
// That will pass the address of the first value element of the block's
// contents.  You could use a later value element, but note that the interface
// as written doesn't have a length limit.  So although you can control where
// it starts, it will keep binding until it hits an end marker.
//

#define Bind_Values_Deep(at,tail,context) \
    Bind_Values_Core((at), (tail), (context), TS_WORD, 0, BIND_DEEP)

#define Bind_Values_All_Deep(at,tail,context) \
    Bind_Values_Core((at), (tail), (context), TS_WORD, TS_WORD, BIND_DEEP)

#define Bind_Values_Shallow(at,tail,context) \
    Bind_Values_Core((at), (tail), (context), TS_WORD, 0, BIND_0)

// Gave this a complex name to warn of its peculiarities.  Calling with
// just BIND_SET is shallow and tricky because the set words must occur
// before the uses (to be applied to bindings of those uses)!
//
#define Bind_Values_Set_Midstream_Shallow(at,tail,context) \
    Bind_Values_Core( \
        (at), (tail), (context), TS_WORD, FLAGIT_KIND(REB_SET_WORD), BIND_0)

#define Unbind_Values_Deep(at,tail) \
    Unbind_Values_Core((at), (tail), nullptr, true)
