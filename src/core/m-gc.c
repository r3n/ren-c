//
//  File: %m-gc.c
//  Summary: "main memory garbage collection"
//  Section: memory
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2019 Ren-C Open Source Contributors
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
// Today's garbage collector is based on a conventional "mark and sweep",
// of REBSER "nodes", which is how it was done in R3-Alpha:
//
//     https://en.wikipedia.org/wiki/Tracing_garbage_collection
//
// A REBVAL's "payload" and "extra" field may or may not contain pointers to
// REBSERs that the GC needs to be aware of.  Some small values like LOGIC!
// or INTEGER! don't, because they can fit the entirety of their data into the
// REBVAL's 4*sizeof(void) cell...though this would change if INTEGER! added
// support for arbitrary-sized-numbers.
//
// Some REBVALs embed REBSER pointers even when the payload would technically
// fit inside their cell.  They do this in order to create a level of
// indirection so that their data can be shared among copies of that REBVAL.
// For instance, HANDLE! does this.
//
// "Deep" marking in R3-Alpha was originally done with recursion, and the
// recursion would stop whenever a mark was hit.  But this meant deeply nested
// structures could quickly wind up overflowing the C stack.  Consider:
//
//     a: copy []
//     repeat 200'000 [a: append copy [] ^a]
//     recycle
//
// The simple solution is that when an unmarked array is hit that it is
// marked and put into a queue for processing (instead of recursed on the
// spot).  This queue is then handled as soon as the marking call is exited,
// and the process repeated until no more items are queued.
//
// !!! There is actually not a specific list of roots of the garbage collect,
// so a first pass of all the REBSER nodes must be done to find them.  This is
// because with the redesigned "RL_API" in Ren-C, ordinary REBSER nodes do
// double duty as lifetime-managed containers for REBVALs handed out by the
// API--without requiring a separate series data allocation.  These could be
// in their own "pool", but that would prevent mingling and reuse among REBSER
// nodes used for other purposes.  Review in light of any new garbage collect
// approaches used.
//

#include "sys-core.h"

#include "sys-int-funcs.h"


// The reason the LINK() and MISC() macros are so weird is because regardless
// of who assigns the fields, the GC wants to be able to mark them.  So the
// same generic field must be used for all cases...which those macros help
// to keep distinct for comprehensibility purposes.
//
// But access via the GC just sees the fields as their generic nodes (though
// through a non-const point of view, to mark them).

#define LINK_Node_TYPE      REBNOD*
#define LINK_Node_CAST

#define MISC_Node_TYPE      REBNOD*
#define MISC_Node_CAST


// !!! In R3-Alpha, the core included specialized structures which required
// their own GC participation.  This is because rather than store their
// properties in conventional Rebol types (like an OBJECT!) they wanted to
// compress their data into a tighter bit pattern than that would allow.
//
// Ren-C has attempted to be increasingly miserly about bytes, and also
// added the ability for C extensions to hook the GC for a cleanup callback
// relating to HANDLE! for any non-Rebol types.  Hopefully this will reduce
// the desire to hook the core garbage collector more deeply.  If a tighter
// structure is desired, that can be done with a HANDLE! or BINARY!, so long
// as any Rebol series/arrays/contexts/functions are done with full values.
//
// Events, Devices, and Gobs are slated to be migrated to structures that
// lean less heavily on C structs and raw C pointers, and leverage higher
// level Rebol services.  So ultimately their implementations would not
// require including specialized code in the garbage collector.  For the
// moment, they still need the hook.
//

static void Mark_Devices_Deep(void);


#ifndef NDEBUG
    static bool in_mark = false; // needs to be per-GC thread
#endif

#define ASSERT_NO_GC_MARKS_PENDING() \
    assert(SER_USED(GC_Mark_Stack) == 0)


static void Queue_Mark_Opt_Value_Deep(const RELVAL *v);

inline static void Queue_Mark_Opt_End_Cell_Deep(const RELVAL *v) {
    if (KIND3Q_BYTE_UNCHECKED(v) != REB_0_END)  // faster than NOT_END()
        Queue_Mark_Opt_Value_Deep(v);
}

inline static void Queue_Mark_Value_Deep(const RELVAL *v)
{
    assert(KIND3Q_BYTE_UNCHECKED(v) != REB_NULL);  // faster than IS_NULLED()
    Queue_Mark_Opt_Value_Deep(v);  // unreadable trash is ok
}


// Ren-C's PAIR! uses a special kind of REBSER that does no additional memory
// allocation, but embeds two REBVALs in the REBSER itself.  A REBVAL has a
// uintptr_t header at the beginning of its struct, just like a REBSER, and
// the NODE_FLAG_MARKED bit is a 0 if unmarked...so it can stealthily
// participate in the marking, as long as the bit is cleared at the end.

// !!! Marking a pairing has the same recursive problems than an array does,
// while not being an array.  So technically we should queue it, but we
// don't have any real world examples of "deeply nested pairings", as they
// are used only in optimized internal structures...the PAIR! datatype only
// allows INTEGER! and DECIMAL! so you can't overflow the stack with it.
//
// Hence we cheat and don't actually queue, for now.
//
static void Queue_Mark_Pairing_Deep(REBVAL *paired)
{
    // !!! Hack doesn't work generically, review

  #if !defined(NDEBUG)
    bool was_in_mark = in_mark;
    in_mark = false;  // would assert about the recursion otherwise
  #endif

    Queue_Mark_Opt_Value_Deep(paired);
    Queue_Mark_Opt_Value_Deep(PAIRING_KEY(paired));

    paired->header.bits |= NODE_FLAG_MARKED;

  #if !defined(NDEBUG)
    in_mark = was_in_mark;
  #endif
}


// This is a generic mark routine, which can sense what type a node is and
// automatically figure out how to mark it.  It takes into account if the
// series was created by an extension and poked nodes into the `custom`
// fields of LINK() and MISC(), which is the only way to "hook" the GC.
//
// (Note: The data structure used for this processing is a "stack" and not
// a "queue".  But when you use 'queue' as a verb, it has more leeway than as
// the CS noun, and can just mean "put into a list for later processing".)
//
static void Queue_Mark_Node_Deep(void *p)
{
    REBYTE first = *cast(const REBYTE*, p);
    if (first & NODE_BYTEMASK_0x10_MARKED)
        return;  // may not be finished marking yet, but has been queued

    if (first & NODE_BYTEMASK_0x01_CELL) {  // e.g. a pairing
        REBVAL *v = VAL(p);
        if (GET_CELL_FLAG(v, MANAGED))
            Queue_Mark_Pairing_Deep(v);
        else {
            // !!! It's a frame?  API handle?  Skip frame case (keysource)
            // for now, but revisit as technique matures.
        }
        return;  // it's 2 cells, sizeof(REBSER), but no room for REBSER data
    }

    REBSER *s = SER(p);
    if (GET_SERIES_FLAG(s, INACCESSIBLE)) {
        //
        // !!! All inaccessible nodes should be collapsed and canonized into
        // a universal inaccessible node so the stub can be freed.  But since
        // bound words depend on contexts to supply their spellings (to free
        // up space in the word cell), we'd need to notice inaccessible word
        // bindings and move their spellings back into them.  (This would
        // make them decay to being unbound, which causes an error, but it
        // would be more helpful to have a flag to indicate their binding
        // went stale...some other cell flag?)  For now, just leave it.
        //
        /*TRASH_POINTER_IF_DEBUG(s->misc.trash);
        TRASH_POINTER_IF_DEBUG(s->link.trash);
        s->header.bits &= ~(
            SERIES_FLAG_LINK_NODE_NEEDS_MARK
                | SERIES_FLAG_MISC_NODE_NEEDS_MARK
        );*/
        s->leader.bits |= NODE_FLAG_MARKED;
        return;
    }

  #if !defined(NDEBUG)
    if (IS_FREE_NODE(s))
        panic (s);

    if (NOT_SERIES_FLAG((s), MANAGED)) {
        printf("Link to non-MANAGED item reached by GC\n");
        panic (s);
    }
  #endif

    s->leader.bits |= NODE_FLAG_MARKED;  // may be already set

    if (GET_SERIES_FLAG(s, LINK_NODE_NEEDS_MARK) and node_LINK(Node, s)) {
        //
        // !!! The keysource for varlists can be set to a REBFRM*, which acts
        // like a cell because the flag is set to being an "endlike header".
        // The DEBUG_CHECK_CASTS noticed that this was marking an END when
        // casting as a SER().  It wasn't entirely obvious what was going on,
        // but this makes it clearer so that a more elegant fix can be made.
        //
        if (Is_Node_Cell(node_LINK(Node, s)))
            if (IS_VARLIST(s))
                goto skip_mark_rebfrm_link;

        REBSER *link = SER(node_LINK(Node, s));
        Queue_Mark_Node_Deep(link);

        // Keylist series need to be marked.
        //
        // !!! Review efficiency, this may need a separate flag for "has
        // pointers that need marking", such lists are used elsewhere.
        //
        if (IS_KEYLIST(link)) {
            REBKEY *tail = SER_TAIL(REBKEY, link);
            REBKEY *key = SER_HEAD(REBKEY, link);
            for (; key != tail; ++key)
                m_cast(REBSYM*, KEY_SYMBOL(key))->leader.bits
                    |= NODE_FLAG_MARKED;
        }
    }

  skip_mark_rebfrm_link:
    if (GET_SERIES_FLAG(s, MISC_NODE_NEEDS_MARK) and node_MISC(Node, s))
        Queue_Mark_Node_Deep(node_MISC(Node, s));

  //=//// MARK INODE (if not using slot for `info`) ///////////////////////=//

    if (GET_SERIES_FLAG(s, INFO_NODE_NEEDS_MARK)) {
        REBNOD *inode = node_INODE(Node, s);
        if (inode) {
          #if !defined(NDEBUG)
            if (IS_POINTER_TRASH_DEBUG(inode))
                panic (s);
          #endif
            Queue_Mark_Node_Deep(inode);
        }
    }

    if (IS_SER_ARRAY(s)) {
        REBARR *a = ARR(s);

    //=//// MARK BONUS (if not using slot for `bias`) /////////////////////=//

        // Whether the bonus slot needs to be marked is dictated by internal
        // series type, not an extension-usable flag (due to flag scarcity).
        //
        if (IS_SER_DYNAMIC(a) and not IS_SER_BIASED(a)) {
            REBNOD *bonus = node_BONUS(Node, a);
            if (bonus) {
              #if !defined(NDEBUG)
                if (IS_POINTER_TRASH_DEBUG(bonus))
                    panic (a);
              #endif
                Queue_Mark_Node_Deep(bonus);
            }
        }

    //=//// MARK ARRAY ELEMENT CELLS (if array) ///////////////////////////=//

        // Submits the array into the deferred stack to be processed later
        // with Propagate_All_GC_Marks().  If it were not queued and just used
        // recursion (as R3-Alpha did) then deeply nested arrays could
        // overflow the C stack.
        //
        // !!! Could the amount of C stack space available be used for some
        // amount of recursion, and only queue if running up against a limit?
        //
        // !!! Should this use a "bumping a NULL at the end" technique to
        // grow, like the data stack?
        //
        if (SER_FULL(GC_Mark_Stack))
            Extend_Series(GC_Mark_Stack, 8);
        *SER_AT(REBARR*, GC_Mark_Stack, SER_USED(GC_Mark_Stack)) = a;
        SET_SERIES_USED(GC_Mark_Stack, SER_USED(GC_Mark_Stack) + 1);  // !term
    }
}


//
//  Queue_Mark_Opt_Value_Deep: C
//
// If a slot is not supposed to allow END, use Queue_Mark_Opt_Value_Deep()
// If a slot allows neither END nor NULLED cells, use Queue_Mark_Value_Deep()
//
static void Queue_Mark_Opt_Value_Deep(const RELVAL *v)
{
    assert(KIND3Q_BYTE_UNCHECKED(v) != REB_0_END);  // faster than NOT_END()

    // We mark based on the type of payload in the cell, e.g. its "unescaped"
    // form.  So if '''a fits in a WORD! (despite being a QUOTED!), we want
    // to mark the cell as if it were a plain word.  Use the CELL_KIND.
    //
    enum Reb_Kind heart = cast(enum Reb_Kind, HEART_BYTE(v));

  #if !defined(NDEBUG)  // see Queue_Mark_Node_Deep() for notes on recursion
    assert(not in_mark);
    in_mark = true;
  #endif

    if (IS_BINDABLE_KIND(heart)) {
        REBSER *binding = BINDING(v);
        if (binding != UNBOUND)
            if (NODE_BYTE(binding) & NODE_BYTEMASK_0x20_MANAGED)
                Queue_Mark_Node_Deep(binding);
    }

    if (GET_CELL_FLAG(v, FIRST_IS_NODE) and VAL_NODE1(v))
        Queue_Mark_Node_Deep(VAL_NODE1(v));

    if (GET_CELL_FLAG(v, SECOND_IS_NODE) and VAL_NODE2(v))
        Queue_Mark_Node_Deep(VAL_NODE2(v));

  #if !defined(NDEBUG)
    in_mark = false;
    Assert_Cell_Marked_Correctly(v);
  #endif
}


//
//  Propagate_All_GC_Marks: C
//
// The Mark Stack is a series containing series pointers.  They have already
// had their SERIES_FLAG_MARK set to prevent being added to the stack multiple
// times, but the items they can reach are not necessarily marked yet.
//
// Processing continues until all reachable items from the mark stack are
// known to be marked.
//
static void Propagate_All_GC_Marks(void)
{
    assert(not in_mark);

    while (SER_USED(GC_Mark_Stack) != 0) {
        SET_SERIES_USED(GC_Mark_Stack, SER_USED(GC_Mark_Stack) - 1);  // safe

        // Data pointer may change in response to an expansion during
        // Mark_Array_Deep_Core(), so must be refreshed on each loop.
        //
        REBARR *a = *SER_AT(REBARR*, GC_Mark_Stack, SER_USED(GC_Mark_Stack));

        // Termination is not required in the release build (the length is
        // enough to know where it ends).  But overwrite with trash in debug.
        //
        TRASH_POINTER_IF_DEBUG(
            *SER_AT(REBARR*, GC_Mark_Stack, SER_USED(GC_Mark_Stack))
        );

        // We should have marked this series at queueing time to keep it from
        // being doubly added before the queue had a chance to be processed
         //
        assert(a->leader.bits & NODE_FLAG_MARKED);

        RELVAL *v = ARR_HEAD(a);
        const RELVAL *tail = ARR_TAIL(a);
        for (; v != tail; ++v) {
            Queue_Mark_Opt_Value_Deep(v);

          #if !defined(NDEBUG)
            //
            // Nulls are illegal in most arrays, but context varlists use
            // "nulled cells" to denote that the variable is not set.
            //
            if (KIND3Q_BYTE_UNCHECKED(v) == REB_NULL) {
                if (not (IS_VARLIST(a) or IS_PATCH(a) or IS_PAIRLIST(a)))
                    panic (a);
            }

            if (
                KIND3Q_BYTE_UNCHECKED(v) == REB_BAD_WORD
                and GET_CELL_FLAG(v, ISOTOPE)
            ){
                // BAD-WORD! isotopes may not exist in blocks, they can only be
                // in objects/frames.
                //
                assert(IS_VARLIST(a) or IS_PATCH(a));
            }
          #endif
        }

      #if !defined(NDEBUG)
        Assert_Array_Marked_Correctly(a);
      #endif
    }
}


//
//  Reify_Va_To_Array_In_Frame: C
//
// For performance and memory usage reasons, a variadic C function call that
// wants to invoke the evaluator with just a comma-delimited list of REBVAL*
// does not need to make a series to hold them.  Eval_Core is written to use
// the va_list traversal as an alternate to DO-ing an ARRAY.
//
// However, va_lists cannot be backtracked once advanced.  So in a debug mode
// it can be helpful to turn all the va_lists into arrays before running
// them, so stack frames can be inspected more meaningfully--both for upcoming
// evaluations and those already past.
//
// Because items may well have already been consumed from the va_list() that
// can't be gotten back, we put in a marker to help hint at the truncation
// (unless told that it's not truncated, e.g. a debug mode that calls it
// before any items are consumed).
//
void Reify_Va_To_Array_In_Frame(
    REBFRM *f,
    bool truncated
) {
    REBDSP dsp_orig = DSP;

    assert(FRM_IS_VARIADIC(f));

    if (truncated) {
        DS_PUSH();
        Init_Word(DS_TOP, Canon(SYM___OPTIMIZED_OUT__));
    }

    REBLEN index;

    if (NOT_END(f_value)) {
        do {
            Derelativize(DS_PUSH(), f_value, f_specifier);
            assert(not IS_NULLED(DS_TOP));
            Fetch_Next_Forget_Lookback(f);
        } while (NOT_END(f_value));

        if (truncated)
            index = 2;  // skip the --optimized-out--
        else
            index = 1;  // position at start of the extracted values
    }
    else {
        assert(FEED_PENDING(f->feed) == nullptr);

        // Leave at end of frame, but give back the array to serve as
        // notice of the truncation (if it was truncated)
        //
        index = 0;
    }

    // feeding forward should have called va_end
    //
    assert(not FEED_IS_VARIADIC(f->feed));

    if (DSP == dsp_orig)
        Init_Block(FEED_SINGLE(f->feed), EMPTY_ARRAY);  // no new array needed
    else {
        REBARR *a = Pop_Stack_Values_Core(dsp_orig, SERIES_FLAG_MANAGED);
        Init_Any_Array_At(FEED_SINGLE(f->feed), REB_BLOCK, a, index);
    }

    if (truncated)
        f->feed->value = ARR_AT(f_array, 1);  // skip trunc
    else
        f->feed->value = ARR_HEAD(f_array);

    // The array just popped into existence, and it's tied to a running
    // frame...so safe to say we're holding it (if not at the end).
    //
    if (IS_END(f_value))
        assert(FEED_PENDING(f->feed) == nullptr);
    else {
        assert(NOT_FEED_FLAG(f->feed, TOOK_HOLD));
        SET_SERIES_INFO(m_cast(REBARR*, f_array), HOLD);
        SET_FEED_FLAG(f->feed, TOOK_HOLD);
    }
}


//
//  Mark_Root_Series: C
//
// Root Series are any manual series that were allocated but have not been
// managed yet, as well as Alloc_Value() nodes that are explicitly "roots".
//
// For root nodes, this checks to see if their lifetime was dependent on a
// FRAME!, and if that frame is no longer on the stack.  If so, it (currently)
// will panic if that frame did not end due to a fail().  This could be
// relaxed to automatically free those nodes as a normal GC.
//
// !!! This implementation walks over *all* the nodes.  It wouldn't have to
// if API nodes were in their own pool, or if the outstanding manuals list
// were maintained even in non-debug builds--it could just walk those.  This
// should be weighed against background GC and other more sophisticated
// methods which might come down the road for the GC than this simple one.
//
static void Mark_Root_Series(void)
{
    REBSEG *seg;
    for (seg = Mem_Pools[SER_POOL].segs; seg; seg = seg->next) {
        REBYTE *unit = cast(REBYTE*, seg + 1);
        REBLEN n = Mem_Pools[SER_POOL].num_units;
        for (; n > 0; --n, unit += sizeof(REBSER)) {
            //
            // !!! A smarter switch statement here could do this more
            // optimally...see the sweep code for an example.
            //
            REBYTE nodebyte = *unit;
            if (nodebyte & NODE_BYTEMASK_0x40_FREE)
                continue;

            assert(nodebyte & NODE_BYTEMASK_0x80_NODE);

            if (nodebyte & NODE_BYTEMASK_0x02_ROOT) {
                //
                // This came from Alloc_Value(); all references should be
                // from the C stack, only this visit should be marking it.
                //
                REBARR *a = ARR(cast(void*, unit));

                assert(not (a->leader.bits & NODE_FLAG_MARKED));

                if (not (a->leader.bits & NODE_FLAG_MANAGED)) {
                    // if it's not managed, don't mark it (don't have to?)
                }
                else  // Note that Mark_Frame_Stack_Deep() will mark the owner
                    a->leader.bits |= NODE_FLAG_MARKED;

                // Note: Eval_Core() might target API cells, uses END
                //
                Queue_Mark_Opt_End_Cell_Deep(ARR_SINGLE(a));
                continue;
            }

            if (nodebyte & NODE_BYTEMASK_0x01_CELL) {  // a pairing
                REBVAL *paired = VAL(cast(void*, unit));
                if (paired->header.bits & NODE_FLAG_MANAGED)
                    continue; // PAIR! or other value will mark it

                assert(!"unmanaged pairings not believed to exist yet");
                Queue_Mark_Opt_Value_Deep(paired);
                Queue_Mark_Opt_Value_Deep(PAIRING_KEY(paired));
            }

            REBSER *s = SER(cast(void*, unit));
            if (IS_SER_ARRAY(s)) {
                if (s->leader.bits & NODE_FLAG_MANAGED)
                    continue; // BLOCK! should mark it

                REBARR *a = ARR(s);

                if (IS_VARLIST(a))
                    if (CTX_TYPE(CTX(a)) == REB_FRAME)
                        continue;  // Mark_Frame_Stack_Deep() etc. mark it

                // This means someone did something like Make_Array() and then
                // ran an evaluation before referencing it somewhere from the
                // root set.

                // Only plain arrays are supported as unmanaged across
                // evaluations, because REBCTX and REBACT and REBMAP are too
                // complex...they must be managed before evaluations happen.
                // Manage and use PUSH_GC_GUARD and DROP_GC_GUARD on them.
                //
                assert(
                    not IS_VARLIST(a)
                    and not IS_DETAILS(a)
                    and not IS_PAIRLIST(a)
                );

                if (GET_SERIES_FLAG(a, LINK_NODE_NEEDS_MARK))
                    if (node_LINK(Node, a))
                        Queue_Mark_Node_Deep(node_LINK(Node, a));
                if (GET_SERIES_FLAG(a, MISC_NODE_NEEDS_MARK))
                    if (node_MISC(Node, a))
                        Queue_Mark_Node_Deep(node_MISC(Node, a));

                const RELVAL *item_tail = ARR_TAIL(a);
                RELVAL *item = ARR_HEAD(a);
                for (; item != item_tail; ++item)
                    Queue_Mark_Value_Deep(item);
            }

            // At present, no handling for unmanaged STRING!, BINARY!, etc.
            // This would have to change, e.g. if any of other types stored
            // something on the heap in their LINK() or MISC()
        }

        Propagate_All_GC_Marks(); // !!! is propagating on each segment good?
    }
}


//
//  Mark_Data_Stack: C
//
// The data stack logic is that it is contiguous values with no END markers
// except at the array end.  Bumping up against that END signal is how the
// stack knows when it needs to grow.
//
// But every drop of the stack doesn't overwrite the dropped value.  Since the
// values are not END markers, they are considered fine as far as a NOT_END()
// test is concerned to indicate unused capacity.  So the values are good
// for the testing purpose, yet the GC doesn't want to consider those to be
// "live" references.  So rather than to a full Queue_Mark_Array_Deep() on
// the capacity of the data stack's underlying array, it begins at DS_TOP.
//
static void Mark_Data_Stack(void)
{
    const RELVAL *head = ARR_HEAD(DS_Array);
    assert(IS_TRASH(head));  // DS_AT(0) is deliberately invalid

    REBVAL *stackval = DS_Movable_Top;
    for (; stackval != head; --stackval)  // stop before DS_AT(0)
        Queue_Mark_Value_Deep(stackval);

    Propagate_All_GC_Marks();
}


//
//  Mark_Symbol_Series: C
//
// Mark symbol series.  These canon words for SYM_XXX are the only ones that
// are never candidates for GC (until shutdown).  All other symbol series may
// go away if no words, parameters, object keys, etc. refer to them.
//
static void Mark_Symbol_Series(void)
{
    REBSTR **canon = SER_HEAD(REBSTR*, PG_Symbol_Canons);
    assert(IS_POINTER_TRASH_DEBUG(*canon)); // SYM_0 for all non-builtin words
    ++canon;
    for (; *canon != nullptr; ++canon)
        (*canon)->leader.bits |= NODE_FLAG_MARKED;

    ASSERT_NO_GC_MARKS_PENDING(); // doesn't ues any queueing
}


//
//  Mark_Natives: C
//
// For each native C implementation, a REBVAL is created during init to
// represent it as an ACTION!.  These are kept in a global array and are
// protected from GC.  It might not technically be necessary to do so for
// all natives, but at least some have their paramlists referenced by the
// core code (such as RETURN).
//
static void Mark_Natives(void)
{
    REBLEN n;
    for (n = 0; n < Num_Natives; ++n) {
        if (Natives[n])  // checking allows recycle during Startup_Natives()
            Queue_Mark_Node_Deep(Natives[n]);
    }

    Propagate_All_GC_Marks();
}


//
//  Mark_Guarded_Nodes: C
//
// Mark series and values that have been temporarily protected from garbage
// collection with PUSH_GC_GUARD.  Subclasses e.g. ARRAY_IS_CONTEXT will
// have their LINK() and MISC() fields guarded appropriately for the class.
//
static void Mark_Guarded_Nodes(void)
{
    REBNOD **np = SER_HEAD(REBNOD*, GC_Guarded);
    REBLEN n = SER_USED(GC_Guarded);
    for (; n > 0; --n, ++np) {
        REBNOD *node = *np;
        if (Is_Node_Cell(node)) {
            //
            // !!! What if someone tried to GC_GUARD a managed paired REBSER?
            //
            Queue_Mark_Opt_End_Cell_Deep(cast(REBVAL*, node));
        }
        else  // a series
            Queue_Mark_Node_Deep(node);

        Propagate_All_GC_Marks();
    }
}


//
//  Mark_Frame_Stack_Deep: C
//
// Mark values being kept live by all call frames.  If a function is running,
// then this will keep the function itself live, as well as the arguments.
// There is also an "out" slot--which may point to an arbitrary REBVAL cell
// on the C stack (and must contain valid GC-readable bits at all times).
//
// Since function argument slots are not pre-initialized, how far the function
// has gotten in its fulfillment must be taken into account.  Only those
// argument slots through points of fulfillment may be GC protected.
//
// This should be called at the top level, and not from inside a
// Propagate_All_GC_Marks().  All marks will be propagated.
//
static void Mark_Frame_Stack_Deep(void)
{
    REBFRM *f = FS_TOP;

    while (true) { // mark all frames (even FS_BOTTOM)
        //
        // Note: MISC_PENDING() should either live in FEED_ARRAY(), or
        // it may be trash (e.g. if it's an apply).  GC can ignore it.
        //
        REBARR *singular = FEED_SINGULAR(f->feed);
        do {
            Queue_Mark_Value_Deep(ARR_SINGLE(singular));
            singular = LINK(Splice, singular);
        } while (singular);

        // END is possible, because the frame could be sitting at the end of
        // a block when a function runs, e.g. `do [zero-arity]`.  That frame
        // will stay on the stack while the zero-arity function is running.
        // The array still might be used in an error, so can't GC it.
        //
        Queue_Mark_Opt_End_Cell_Deep(f->feed->value);

        // If ->gotten is set, it usually shouldn't need markeding because
        // it's fetched via f->value and so would be kept alive by it.  Any
        // code that a frame runs that might disrupt that relationship so it
        // would fetch differently should have meant clearing ->gotten.
        //
        if (f_gotten)
            assert(f_gotten == Lookup_Word(f_value, f_specifier));

        if (
            f_specifier != SPECIFIED
            and (f_specifier->leader.bits & NODE_FLAG_MANAGED)
        ){
            Queue_Mark_Node_Deep(f_specifier);
        }

        // f->out can be nullptr at the moment, when a frame is created that
        // can ask for a different output each evaluation.
        //
        if (f->out)
            Queue_Mark_Opt_End_Cell_Deep(f->out);

        // Frame temporary cell should always contain initialized bits, as
        // DECLARE_FRAME sets it up and no one is supposed to trash it.
        //
        Queue_Mark_Opt_End_Cell_Deep(&f->feed->fetched);
        Queue_Mark_Opt_End_Cell_Deep(&f->feed->lookback);
        Queue_Mark_Opt_End_Cell_Deep(&f->spare);

        if (not Is_Action_Frame(f)) {
            //
            // Consider something like `eval copy '(recycle)`, because
            // while evaluating the group it has no anchor anywhere in the
            // root set and could be GC'd.  The Reb_Frame's array ref is it.
            //
            goto propagate_and_continue;
        }

        Queue_Mark_Node_Deep(f->original);  // never nullptr

        if (f->label)  // nullptr if anonymous
            Queue_Mark_Node_Deep(m_cast(REBSYM*, unwrap(f->label)));

        // param can be used to GC protect an arbitrary value while a
        // function is running, currently.  nullptr is permitted as well
        // (e.g. path frames use nullptr to indicate no set value on a path)
        //
        if (f->key != f->key_tail and f->param)
            Queue_Mark_Opt_End_Cell_Deep(f->param);

        if (f->varlist and GET_SERIES_FLAG(f->varlist, MANAGED)) {
            //
            // If the context is all set up with valid values and managed,
            // then it can just be marked normally...no need to do custom
            // partial parameter traversal.
            //
            assert(not Is_Action_Frame_Fulfilling(f));
            Queue_Mark_Node_Deep(f->varlist);  // may not pass CTX() test
            goto propagate_and_continue;
        }

        if (f->varlist and GET_SERIES_FLAG(f->varlist, INACCESSIBLE)) {
            //
            // This happens in Encloser_Dispatcher(), where it can capture a
            // varlist that may not be managed (e.g. if there were no ADAPTs
            // or other phases running that triggered it).
            //
            goto propagate_and_continue;
        }

        // Mark arguments as used, but only as far as parameter filling has
        // gotten (may be garbage bits past that).  Could also be an END value
        // of an in-progress arg fulfillment, but in that case it is protected
        // by the *evaluating frame's f->out* (!)
        //
        // Refinements need special treatment, and also consideration of if
        // this is the "doing pickups" or not.  If doing pickups then skip the
        // cells for pending refinement arguments.
        //
        REBACT *phase; // goto would cross initialization
        phase = FRM_PHASE(f);
        const REBKEY *key;
        const REBKEY *tail;
        key = ACT_KEYS(&tail, phase);

        REBVAL *arg;
        for (arg = FRM_ARGS_HEAD(f); key != tail; ++key, ++arg) {
            if (key == f->key) {
                //
                // When key and f->key match, that means that arg is the
                // output slot for some other frame's f->out.  Let that frame
                // do the marking (which tolerates END, an illegal state for
                // prior arg slots we've visited...unless deferred!)

                // If we're not doing "pickups" then the cell slots after
                // this one have not been initialized, not even to trash.
                //
                if (NOT_EVAL_FLAG(f, DOING_PICKUPS))
                    break;

                // But since we *are* doing pickups, we must have initialized
                // all the cells to something...even to trash.  Continue and
                // mark them.
                //
                continue;
            }

            Queue_Mark_Opt_Value_Deep(arg);
        }

      propagate_and_continue:;

        Propagate_All_GC_Marks();
        if (f == FS_BOTTOM)
            break;

        f = f->prior;
    }
}


//
//  Sweep_Series: C
//
// Scans all series nodes (REBSER structs) in all segments that are part of
// the SER_POOL.  If a series had its lifetime management delegated to the
// garbage collector with Manage_Series(), then if it didn't get "marked" as
// live during the marking phase then free it.
//
static REBLEN Sweep_Series(void)
{
    REBLEN count = 0;

    REBSEG *seg = Mem_Pools[SER_POOL].segs;
    for (; seg != nullptr; seg = seg->next) {
        REBLEN n = Mem_Pools[SER_POOL].num_units;

        // We use a generic byte pointer (unsigned char*) to dodge the rules
        // for strict aliasing, as the pool may contain pairs of REBVAL from
        // Alloc_Pairing(), or a REBSER from Alloc_Series_Node().  The shared
        // first byte node masks are defined and explained in %sys-rebnod.h
        //
        // NOTE: If you are using a build with UNUSUAL_REBVAL_SIZE such as
        // DEBUG_TRACK_EXTEND_CELLS, then this will be processing the REBSER
        // nodes only--see loop lower down for the pairing pool enumeration.

        REBYTE *unit = cast(REBYTE*, seg + 1);

        for (; n > 0; --n, unit += sizeof(REBSER)) {
            switch (*unit >> 4) {
              case 0:
              case 1:  // 0x1
              case 2:  // 0x2
              case 3:  // 0x2 + 0x1
              case 4:  // 0x4
              case 5:  // 0x4 + 0x1
              case 6:  // 0x4 + 0x2
              case 7:  // 0x4 + 0x2 + 0x1
                //
                // NODE_FLAG_NODE (0x8) is clear.  This signature is
                // reserved for UTF-8 strings (corresponding to valid ASCII
                // values in the first byte).
                //
                panic (unit);

            // v-- Everything below here has NODE_FLAG_NODE set (0x8)

              case 8:
                // 0x8: unmanaged and unmarked, e.g. a series that was made
                // with Make_Series() and hasn't been managed.  It doesn't
                // participate in the GC.  Leave it as is.
                //
                // !!! Are there actually legitimate reasons to do this with
                // arrays, where the creator knows the cells do not need
                // GC protection?  Should finding an array in this state be
                // considered a problem (e.g. the GC ran when you thought it
                // couldn't run yet, hence would be able to free the array?)
                //
                break;

              case 9:
                // 0x8 + 0x1: marked but not managed, this can't happen,
                // because the marking itself asserts nodes are managed.
                //
                panic (unit);

              case 10:
                // 0x8 + 0x2: managed but didn't get marked, should be GC'd
                //
                // !!! It would be nice if we could have NODE_FLAG_CELL here
                // as part of the switch, but see its definition for why it
                // is at position 8 from left and not an earlier bit.
                //
                if (*unit & NODE_BYTEMASK_0x01_CELL) {
                    assert(not (*unit & NODE_BYTEMASK_0x02_ROOT));
                    Free_Node(SER_POOL, NOD(unit));  // Free_Pairing manual
                }
                else {
                    REBSER *s = cast(REBSER*, unit);
                    GC_Kill_Series(s);
                }
                ++count;
                break;

              case 11:
                // 0x8 + 0x2 + 0x1: managed and marked, so it's still live.
                // Don't GC it, just clear the mark.
                //
                *unit &= ~NODE_BYTEMASK_0x10_MARKED;
                break;

            // v-- Everything below this line has the two leftmost bits set
            // in the header.  In the *general* case this could be a valid
            // first byte of a multi-byte sequence in UTF-8...so only the
            // special bit pattern of the free case uses this.

              case 12:
                // 0x8 + 0x4: free node, uses special illegal UTF-8 byte
                //
                assert(*unit == FREED_SERIES_BYTE);
                break;

              case 13:
              case 14:
              case 15:
                panic (unit);  // 0x8 + 0x4 + ... reserved for UTF-8
            }
        }
    }

    // For efficiency of memory use, REBSER is nominally defined as
    // 2*sizeof(REBVAL), and so pairs can use the same nodes.  But features
    // that might make the cells a size greater than REBSER size require
    // doing pairings in a different pool.
    //
  #ifdef UNUSUAL_REBVAL_SIZE
    for (seg = Mem_Pools[PAR_POOL].segs; seg != NULL; seg = seg->next) {
        REBVAL *v = cast(REBVAL*, seg + 1);
        REBLEN n = Mem_Pools[PAR_POOL].num_units;
        for (; n > 0; --n, v += 2) {
            if (v->header.bits & NODE_FLAG_FREE) {
                assert(FIRST_BYTE(v->header) == FREED_SERIES_BYTE);
                continue;
            }

            assert(v->header.bits & NODE_FLAG_CELL);

            if (v->header.bits & NODE_FLAG_MANAGED) {
                assert(not (v->header.bits & NODE_FLAG_ROOT));
                if (v->header.bits & NODE_FLAG_MARKED)
                    v->header.bits &= ~NODE_FLAG_MARKED;
                else {
                    Free_Node(PAR_POOL, NOD(v));  // Free_Pairing is for manuals
                    ++count;
                }
            }
        }
    }
  #endif

    return count;
}


#if !defined(NDEBUG)

//
//  Fill_Sweeplist: C
//
REBLEN Fill_Sweeplist(REBSER *sweeplist)
{
    assert(SER_WIDE(sweeplist) == sizeof(REBNOD*));
    assert(SER_USED(sweeplist) == 0);

    REBLEN count = 0;

    REBSEG *seg;
    for (seg = Mem_Pools[SER_POOL].segs; seg != NULL; seg = seg->next) {
        REBYTE *unit = cast(REBYTE*, seg + 1);
        REBLEN n = Mem_Pools[SER_POOL].num_units;
        for (; n > 0; --n, unit += sizeof(REBSER)) {
            switch (*unit >> 4) {
              case 9: {  // 0x8 + 0x1
                REBSER *s = SER(cast(void*, unit));
                ASSERT_SERIES_MANAGED(s);
                if (s->leader.bits & NODE_FLAG_MARKED)
                    s->leader.bits &= ~NODE_FLAG_MARKED;
                else {
                    EXPAND_SERIES_TAIL(sweeplist, 1);
                    *SER_AT(REBNOD*, sweeplist, count) = s;
                    ++count;
                }
                break; }

              case 11: {  // 0x8 + 0x2 + 0x1
                //
                // It's a cell which is managed where the value is not an END.
                // This is a managed pairing, so mark bit should be heeded.
                //
                // !!! It is a REBNOD, but *not* a "series".
                //
                REBVAL *pairing = VAL(cast(void*, unit));
                assert(pairing->header.bits & NODE_FLAG_MANAGED);
                if (pairing->header.bits & NODE_FLAG_MARKED)
                    pairing->header.bits &= ~NODE_FLAG_MARKED;
                else {
                    EXPAND_SERIES_TAIL(sweeplist, 1);
                    *SER_AT(REBNOD*, sweeplist, count) = pairing;
                    ++count;
                }
                break; }
            }
        }
    }

    return count;
}

#endif


//
//  Recycle_Core: C
//
// Recycle memory no longer needed.  If sweeplist is not NULL, then it needs
// to be a series whose width is sizeof(REBSER*), and it will be filled with
// the list of series that *would* be recycled.
//
REBLEN Recycle_Core(bool shutdown, REBSER *sweeplist)
{
    // Ordinarily, it should not be possible to spawn a recycle during a
    // recycle.  But when debug code is added into the recycling code, it
    // could cause a recursion.  Be tolerant of such recursions to make that
    // debugging easier...but make a note that it's not ordinarily legal.
    //
  #if !defined(NDEBUG)
    if (GC_Recycling) {
        printf("Recycle re-entry; should only happen in debug scenarios.\n");
        SET_SIGNAL(SIG_RECYCLE);
        return 0;
    }
  #endif

    // It is currently assumed that no recycle will happen while in a thrown
    // state.  Debug calls that do evaluation (or even Recycle() directly)
    // between the time a function has been called and the throw is handled
    // can cause problems with this.
    //
    assert(IS_END(&TG_Thrown_Arg));
  #if !defined(NDEBUG)
    assert(IS_END(&TG_Thrown_Label_Debug));
  #endif

    // If disabled by RECYCLE/OFF, exit now but set the pending flag.  (If
    // shutdown, ignore so recycling runs and can be checked for balance.)
    //
    if (not shutdown and GC_Disabled) {
        SET_SIGNAL(SIG_RECYCLE);
        return 0;
    }

  #if !defined(NDEBUG)
    GC_Recycling = true;
  #endif

    ASSERT_NO_GC_MARKS_PENDING();

  #if defined(DEBUG_COLLECT_STATS)
    PG_Reb_Stats->Recycle_Counter++;
    PG_Reb_Stats->Recycle_Series = Mem_Pools[SER_POOL].free;
    PG_Reb_Stats->Mark_Count = 0;
  #endif

    // The TG_Reuse list consists of entries which could grow to arbitrary
    // length, and which aren't being tracked anywhere.  Cull them during GC
    // in case the stack at one point got very deep and isn't going to use
    // them again, and the memory needs reclaiming.
    //
    while (TG_Reuse) {
        REBARR *varlist = TG_Reuse;
        TG_Reuse = LINK(ReuseNext, TG_Reuse);
        GC_Kill_Series(varlist); // no track for Free_Unmanaged_Series()
    }

    // MARKING PHASE: the "root set" from which we determine the liveness
    // (or deadness) of a series.  If we are shutting down, we do not mark
    // several categories of series...but we do need to run the root marking.
    // (In particular because that is when API series whose lifetimes
    // are bound to frames will be freed, if the frame is expired.)
    //
    Mark_Root_Series();

    if (not shutdown) {
        Mark_Natives();
        Mark_Symbol_Series();

        Mark_Data_Stack();

        Mark_Guarded_Nodes();

        Mark_Frame_Stack_Deep();

        Propagate_All_GC_Marks();

        Mark_Devices_Deep();
    }

    // SWEEPING PHASE

    ASSERT_NO_GC_MARKS_PENDING();

    REBLEN count = 0;

    if (sweeplist != NULL) {
    #if defined(NDEBUG)
        panic (sweeplist);
    #else
        count += Fill_Sweeplist(sweeplist);
    #endif
    }
    else
        count += Sweep_Series();

  #if defined(DEBUG_COLLECT_STATS)
    // Compute new stats:
    PG_Reb_Stats->Recycle_Series
        = Mem_Pools[SER_POOL].free - PG_Reb_Stats->Recycle_Series;
    PG_Reb_Stats->Recycle_Series_Total += PG_Reb_Stats->Recycle_Series;
    PG_Reb_Stats->Recycle_Prior_Eval = Eval_Cycles;
  #endif

    // !!! This reset of the "ballast" is the original code from R3-Alpha:
    //
    // https://github.com/rebol/rebol/blob/25033f897b2bd466068d7663563cd3ff64740b94/src/core/m-gc.c#L599
    //
    // Atronix R3 modified it, but that modification created problems:
    //
    // https://github.com/zsx/r3/issues/32
    //
    // Reverted to the R3-Alpha state, accommodating a comment "do not adjust
    // task variables or boot strings in shutdown when they are being freed."
    //
    if (not shutdown)
        GC_Ballast = TG_Ballast;

    ASSERT_NO_GC_MARKS_PENDING();

  #if !defined(NDEBUG)
    GC_Recycling = false;
  #endif

  #if !defined(NDEBUG)
    //
    // This might be an interesting feature for release builds, but using
    // normal I/O here that runs evaluations could be problematic.  Even
    // though we've finished the recycle, we're still in the signal handling
    // stack, so calling into the evaluator e.g. for rebPrint() may be bad.
    //
    if (Reb_Opts->watch_recycle) {
        printf("RECYCLE: %u nodes\n", cast(unsigned int, count));
        fflush(stdout);
    }
  #endif

    return count;
}


//
//  Recycle: C
//
// Recycle memory no longer needed.
//
REBLEN Recycle(void)
{
    // Default to not passing the `shutdown` flag.
    //
    REBLEN n = Recycle_Core(false, NULL);

  #ifdef DOUBLE_RECYCLE_TEST
    //
    // If there are two recycles in a row, then the second should not free
    // any additional series that were not freed by the first.  (It also
    // shouldn't crash.)  This is an expensive check, but helpful to try if
    // it seems a GC left things in a bad state that crashed a later GC.
    //
    REBLEN n2 = Recycle_Core(false, NULL);
    assert(n2 == 0);
  #endif

    return n;
}


//
//  Push_Guard_Node: C
//
void Push_Guard_Node(const REBNOD *node)
{
  #if !defined(NDEBUG)
    if (NODE_BYTE(node) & NODE_BYTEMASK_0x01_CELL) {
        //
        // It is a value.  Cheap check: require that it already contain valid
        // data when the guard call is made (even if GC isn't necessarily
        // going to happen immediately, and value could theoretically become
        // valid before then.)
        //
        const REBVAL* v = cast(const REBVAL*, node);
        assert(CELL_KIND_UNCHECKED(v) < REB_MAX);

      #ifdef STRESS_CHECK_GUARD_VALUE_POINTER
        //
        // Technically we should never call this routine to guard a value
        // that lives inside of a series.  Not only would we have to guard the
        // containing series, we would also have to lock the series from
        // being able to resize and reallocate the data pointer.  But this is
        // a somewhat expensive check, so only feasible to run occasionally.
        //
        REBNOD *containing = Try_Find_Containing_Node_Debug(v);
        if (containing)
            panic (containing);
      #endif
    }
    else {
        // It's a series.  Does not ensure the series being guarded is
        // managed, since it can be interesting to guard the managed
        // *contents* of an unmanaged array.  The calling wrappers ensure
        // managedness or not.
    }
  #endif

    if (SER_FULL(GC_Guarded))
        Extend_Series(GC_Guarded, 8);

    *SER_AT(const REBNOD*, GC_Guarded, SER_USED(GC_Guarded)) = node;

    SET_SERIES_USED(GC_Guarded, SER_USED(GC_Guarded) + 1);
}


//
//  Startup_GC: C
//
// Initialize garbage collector.
//
void Startup_GC(void)
{
    assert(not GC_Disabled);
    assert(not GC_Recycling);

    GC_Ballast = MEM_BALLAST;

    // Temporary series and values protected from GC. Holds node pointers.
    //
    GC_Guarded = Make_Series(15, FLAG_FLAVOR(NODELIST));

    // The marking queue used in lieu of recursion to ensure that deeply
    // nested structures don't cause the C stack to overflow.
    //
    GC_Mark_Stack = Make_Series(100, FLAG_FLAVOR(NODELIST));
}


//
//  Shutdown_GC: C
//
void Shutdown_GC(void)
{
    Free_Unmanaged_Series(GC_Guarded);
    Free_Unmanaged_Series(GC_Mark_Stack);
}


//
//  Mark_Devices_Deep: C
//
// Mark all devices. Search for pending requests.
//
// This should be called at the top level, and as it is not
// 'Queued' it guarantees that the marks have been propagated.
//
static void Mark_Devices_Deep(void)
{
    REBDEV *dev = PG_Device_List;

    for (; dev != nullptr; dev = dev->next) {
        if (not dev->pending)
            continue;

        REBNOD *req = m_cast(REBNOD*, dev->pending);

        // This used to walk the ->next field of the REBREQ explicitly, and
        // mark the port pointers internal to the REBREQ.  Following the
        // links and marking the contexts is now done automatically, because
        // REBREQ is a REBSER node and has those fields in LINK()/MISC() with
        // SERIES_FLAG_LINK_NODE_NEEDS_MARK/SERIES_FLAG_MISC_NODE_NEEDS_MARK
        //
        Queue_Mark_Node_Deep(req);
    }

    Propagate_All_GC_Marks();
}
