//
//  File: %sys-feed.h
//  Summary: {Accessors and Argument Pushers/Poppers for Function Call Frames}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2018-2019 Ren-C Open Source Contributors
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
// A "Feed" represents an abstract source of Rebol values, which only offers
// a guarantee of being able to have two sequential values in the feed as
// having valid pointers at one time.  The main pointer is the feed's value
// (feed->value), and to be able to have another pointer to the previous
// value one must request a "lookback" at the time of advancing the feed.
//
// One reason for the feed's strict nature is that it offers an interface not
// just to Rebol BLOCK!s and other arrays, but also to variadic lists such
// as C's va_list...in a system which also allows the mixure of portions of
// UTF-8 string source text.  C's va_list does not retain a memory of the
// past, so once va_arg() is called it forgets the previous value...and
// since values may also be fabricated from text it can get complicated.
//
// Another reason for the strictness is to help rein in the evaluator design
// to keep it within a certain boundary of complexity.


#if !defined(__cplusplus)
    #define QUOTING_BYTE(feed) \
        mutable_SECOND_BYTE((feed)->flags.bits)
#else
    inline static REBYTE& QUOTING_BYTE(REBFED *feed)  // type checks feed...
        { return mutable_SECOND_BYTE(feed->flags.bits); }  // ...but mutable
#endif


#define FEED_SINGULAR(feed)     ARR(&(feed)->singular)
#define FEED_SINGLE(feed)       &(feed)->singular.content.fixed.values[0]

#define LINK_SPLICE(s)          *cast(REBARR**, &LINK(s).custom.node)
#define MISC_PENDING(s)         *cast(const RELVAL**, &MISC(s).custom.node)

#define FEED_SPLICE(feed)       LINK_SPLICE(&(feed)->singular)

// This contains an IS_END() marker if the next fetch should be an attempt
// to consult the va_list (if any).  That end marker may be resident in
// an array, or if it's a plain va_list source it may be the global END.
//
#define FEED_PENDING(feed)      MISC_PENDING(&(feed)->singular)


// For performance, we always get the specifier from the same location, even
// if we're not using an array.  So for the moment, that means using a
// COMMA! (which for technical reasons has a nullptr binding and is thus
// always SPECIFIED).  However, VAL_SPECIFIER() only runs on arrays, so
// we sneak past that by accessing the node directly.
//
#define FEED_SPECIFIER(feed) \
    EXTRA(Binding, FEED_SINGLE(feed)).node

#define FEED_ARRAY(feed) \
    VAL_ARRAY(FEED_SINGLE(feed))

#define FEED_INDEX(feed) \
    VAL_INDEX_UNBOUNDED(FEED_SINGLE(feed))


// Ordinary Rebol internals deal with REBVAL* that are resident in arrays.
// But a va_list can contain UTF-8 string components or special instructions
// that are other Detect_Rebol_Pointer() types.  Anyone who wants to set or
// preload a frame's state for a va_list has to do this detection, so this
// code has to be factored out to just take a void* (because a C va_list
// cannot have its first parameter in the variadic, va_list* is insufficient)
//
inline static const RELVAL *Detect_Feed_Pointer_Maybe_Fetch(
    REBFED *feed,
    const void *p,
    bool preserve
){
    const RELVAL *lookback;

    if (not preserve)
        lookback = nullptr;
    else {
        assert(READABLE(feed->value, __FILE__, __LINE__));  // ensure cell

        if (GET_CELL_FLAG(&feed->fetched, FETCHED_MARKED_TEMPORARY)) {
            //
            // f->value was transient and hence constructed into f->fetched.
            // We may overwrite it below for this fetch.  So save the old one
            // into f->lookback, where it will be safe until the next fetch.
            //
            assert(feed->value == &feed->fetched);
            lookback = Move_Value(&feed->lookback, SPECIFIC(&feed->fetched));
        }
        else {
            // pointer they had should be stable, GC-safe

            lookback = feed->value;
        }
    }

  detect_again:;

    TRASH_POINTER_IF_DEBUG(feed->value);  // should be assigned below

    if (not p) {  // libRebol's null/<opt> (IS_NULLED prohibited in CELL case)

        if (QUOTING_BYTE(feed) == 0)
            panic ("Cannot directly splice nulls...use rebQ(), rebXxxQ()");

        // !!! We could make a global QUOTED_NULLED_VALUE with a stable
        // pointer and not have to use fetched or FETCHED_MARKED_TEMPORARY.
        //
        Init_Comma(FEED_SINGLE(feed));  // for VAL_SPECIFIER() = SPECIFIED

        Quotify(Init_Nulled(&feed->fetched), 1);
        SET_CELL_FLAG(&feed->fetched, FETCHED_MARKED_TEMPORARY);
        feed->value = &feed->fetched;

    } else switch (Detect_Rebol_Pointer(p)) {

      case DETECTED_AS_UTF8: {
        REBDSP dsp_orig = DSP;

        // Allocate space for a binder, but don't initialize it until needed
        // (e.g. a WORD! is seen in a text portion).  This way things like
        // `rebElide(foo_func, "1")` or `block = rebValue("[", item, "]")`
        // won't trigger it.
        //
        // Note that the binder is only used on loaded text.  The scanner
        // leaves all spliced values with whatever bindings they have (even
        // if that is none).
        //
        // !!! Some kind of "binding instruction" might allow other uses?
        //
        struct Reb_Binder binder;
        feed->binder = &binder;
        feed->context = nullptr;  // made non-nullptr when binder initialized
        feed->lib = nullptr;

        Init_Comma(FEED_SINGLE(feed));  // for VAL_SPECIFIER() = SPECIFIED

        SCAN_LEVEL level;
        SCAN_STATE ss;
        const REBLIN start_line = 1;
        Init_Va_Scan_Level_Core(
            &level,
            &ss,
            Intern_Unsized_Managed("sys-do.h"),
            start_line,
            cast(const REBYTE*, p),
            feed
        );

        REBVAL *error = rebRescue(cast(REBDNG*, &Scan_To_Stack), &level);
        if (feed->context)
            Shutdown_Interning_Binder(&binder, unwrap(feed->context));

        if (error) {
            REBCTX *error_ctx = VAL_CONTEXT(error);
            rebRelease(error);
            fail (error_ctx);
        }

        if (DSP == dsp_orig) {
            //
            // This happens when somone says rebValue(..., "", ...) or similar,
            // and gets an empty array from a string scan.  It's not legal
            // to put an END in f->value, and it's unknown if the variadic
            // feed is actually over so as to put null... so get another
            // value out of the va_list and keep going.
            //
            if (feed->vaptr)
                p = va_arg(*unwrap(feed->vaptr), const void*);
            else
                p = *feed->packed++;
            goto detect_again;
        }

        // !!! for now, assume scan went to the end; ultimately it would need
        // to pass the feed in as a parameter for partial scans
        //
        if (feed->vaptr)
            feed->vaptr = nullptr;
        else
            feed->packed = nullptr;

        REBARR *reified = Pop_Stack_Values(dsp_orig);

        // !!! We really should be able to free this array without managing it
        // when we're done with it, though that can get a bit complicated if
        // there's an error or need to reify into a value.  For now, do the
        // inefficient thing and manage it.
        //
        // !!! Scans that produce only one value (which are likely very
        // common) can go into feed->fetched and not make an array at all.
        //
        Manage_Array(reified);

        feed->value = STABLE(ARR_HEAD(reified));
        FEED_PENDING(feed) = feed->value + 1;  // may be END
        Init_Any_Array_At(FEED_SINGLE(feed), REB_BLOCK, reified, 1);

        CLEAR_CELL_FLAG(&feed->fetched, FETCHED_MARKED_TEMPORARY);
        break; }

      case DETECTED_AS_SERIES: {  // e.g. rebQ, rebU, or a rebR() handle
        REBARR *inst1 = ARR(m_cast(void*, p));

        // As we feed forward, we're supposed to be freeing this--it is not
        // managed -and- it's not manuals tracked, it is only held alive by
        // the va_list()'s plan to visit it.  A fail() here won't auto free
        // it *because it is this traversal code which is supposed to free*.
        //
        // !!! Actually, THIS CODE CAN'T FAIL.  :-/  It is part of the
        // implementation of fail's cleanup itself.
        //
        if (GET_ARRAY_FLAG(inst1, INSTRUCTION_ADJUST_QUOTING)) {
            assert(NOT_SERIES_FLAG(inst1, MANAGED));

            if (QUOTING_BYTE(feed) + MISC(inst1).quoting_delta < 0)
                panic ("rebU() can't unquote a feed splicing plain values");

            assert(ARR_LEN(inst1) > 0);
            if (ARR_LEN(inst1) > 1)
                panic ("rebU() of more than one value splice not written");

            REBVAL *single = SPECIFIC(STABLE(ARR_SINGLE(inst1)));
            Move_Value(&feed->fetched, single);
            Quotify(
                &feed->fetched,
                QUOTING_BYTE(feed) + MISC(inst1).quoting_delta
            );
            SET_CELL_FLAG(&feed->fetched, FETCHED_MARKED_TEMPORARY);
            feed->value = &feed->fetched;

            GC_Kill_Series(SER(inst1));  // not manuals-tracked
        }
        else if (GET_ARRAY_FLAG(inst1, SINGULAR_API_RELEASE)) {
            //
            // !!! Originally this asserted it was a managed handle, but the
            // needs of API-TRANSIENT are such that a handle which outlives
            // the frame is returned as a SINGULAR_API_RELEASE.  Review.
            //
            /*assert(GET_SERIES_FLAG(inst1, MANAGED));*/

            // See notes above (duplicate code, fix!) about how we might like
            // to use the as-is value and wait to free until the next cycle
            // vs. putting it in fetched/MARKED_TEMPORARY...but that makes
            // this more convoluted.  Review.

            REBVAL *single = SPECIFIC(STABLE(ARR_SINGLE(inst1)));
            Move_Value(&feed->fetched, single);
            Quotify(&feed->fetched, QUOTING_BYTE(feed));
            SET_CELL_FLAG(&feed->fetched, FETCHED_MARKED_TEMPORARY);
            feed->value = &feed->fetched;
            rebRelease(single);  // *is* the instruction
        }
        else
            panic (inst1);

        break; }

      case DETECTED_AS_CELL: {
        const REBVAL *cell = cast(const REBVAL*, p);
        assert(not IS_RELATIVE(cast(const RELVAL*, cell)));

        Init_Comma(FEED_SINGLE(feed));  // for VAL_SPECIFIER() = SPECIFIED

        if (IS_NULLED(cell))  // API enforces use of C's nullptr (0) for NULL
            assert(!"NULLED cell API leak, see NULLIFY_NULLED() in C source");

        if (QUOTING_BYTE(feed) == 0) {
            feed->value = cell;  // cell can be used as-is
        }
        else {
            // We don't want to corrupt the value itself.  We have to move
            // it into the fetched cell and quote it.
            //
            Quotify(Move_Value(&feed->fetched, cell), QUOTING_BYTE(feed));
            SET_CELL_FLAG(&feed->fetched, FETCHED_MARKED_TEMPORARY);
            feed->value = &feed->fetched;  // note END is detected separately
        }
        break; }

      case DETECTED_AS_END: {  // end of variadic input, so that's it for this
        feed->value = END_NODE;
        TRASH_POINTER_IF_DEBUG(FEED_PENDING(feed));

        // The va_end() is taken care of here, or if there is a throw/fail it
        // is taken care of by Abort_Frame_Core()
        //
        if (feed->vaptr) {
            va_end(*unwrap(feed->vaptr));
            feed->vaptr = nullptr;
        }
        else {
            assert(feed->packed);
            feed->packed = nullptr;
        }

        // !!! Error reporting expects there to be an array.  The whole story
        // of errors when there's a va_list is not told very well, and what
        // will have to likely happen is that in debug modes, all va_list
        // are reified from the beginning, else there's not going to be
        // a way to present errors in context.  Fake an empty array for now.
        //
        Init_Block(FEED_SINGLE(feed), EMPTY_ARRAY);

        CLEAR_CELL_FLAG(&feed->fetched, FETCHED_MARKED_TEMPORARY);  // needed?
        break; }

      case DETECTED_AS_FREED_SERIES:
      case DETECTED_AS_FREED_CELL:
      default:
        panic (p);
    }

    return lookback;
}


//
// Fetch_Next_In_Feed_Core() (see notes above)
//
// Once a va_list is "fetched", it cannot be "un-fetched".  Hence only one
// unit of fetch is done at a time, into f->value.  FEED_PENDING() thus
// must hold a signal that data remains in the va_list and it should be
// consulted further.  That signal is an END marker.
//
// More generally, an END marker in FEED_PENDING() for this routine is a
// signal that the vaptr (if any) should be consulted next.
//
inline static const RELVAL *Fetch_Next_In_Feed_Core(
    REBFED *feed,
    bool preserve
){
  #ifdef DEBUG_EXPIRED_LOOKBACK
    if (feed->stress) {
        TRASH_CELL_IF_DEBUG(feed->stress);
        free(feed->stress);
        feed->stress = nullptr;
    }
  #endif

    // We are changing ->value, and thus by definition any ->gotten value
    // will be invalid.  It might be "wasteful" to always set this to null,
    // especially if it's going to be overwritten with the real fetch...but
    // at a source level, having every call to Fetch_Next_In_Frame have to
    // explicitly set ->gotten to null is overkill.  Could be split into
    // a version that just trashes ->gotten in the debug build vs. null.
    //
    feed->gotten = nullptr;

    const RELVAL *lookback;

    if (NOT_END(FEED_PENDING(feed))) {
        //
        // We assume the ->pending value lives in a source array, and can
        // just be incremented since the array has SERIES_INFO_HOLD while it
        // is being executed hence won't be relocated or modified.  This
        // means the release build doesn't need to call ARR_AT().
        //
        assert(
            FEED_PENDING(feed) == ARR_AT(FEED_ARRAY(feed), FEED_INDEX(feed))
        );

        lookback = feed->value;  // should have been stable
        feed->value = FEED_PENDING(feed);

        ++FEED_PENDING(feed);  // might be becoming an END marker, here
        ++FEED_INDEX(feed);
    }
    else if (feed->vaptr) {
        //
        // A variadic can source arbitrary pointers, which can be detected
        // and handled in different ways.  Notably, a UTF-8 string can be
        // differentiated and loaded.
        //
        const void *p = va_arg(*unwrap(feed->vaptr), const void*);
       // feed->index = TRASHED_INDEX; // avoids warning in release build
        lookback = Detect_Feed_Pointer_Maybe_Fetch(feed, p, preserve);
    }
    else if (feed->packed) {
        //
        // C++ variadics use an ordinary packed array of pointers, because
        // they do more ambitious things with the arguments and there is no
        // (standard) way to construct a C va_list programmatically.
        //
        const void *p = *feed->packed++;
        lookback = Detect_Feed_Pointer_Maybe_Fetch(feed, p, preserve);
    }
    else {
        // The frame was either never variadic, or it was but got spooled into
        // an array by Reify_Va_To_Array_In_Frame().  The first END we hit
        // is the full stop end.

        lookback = feed->value;
        feed->value = END_NODE;
        TRASH_POINTER_IF_DEBUG(FEED_PENDING(feed));

        ++FEED_INDEX(feed);  // for consistency in index termination state
    }

    assert(
        IS_END(feed->value)
        or NOT_CELL_FLAG(&feed->fetched, FETCHED_MARKED_TEMPORARY)
        or feed->value == &feed->fetched
    );

  #ifdef DEBUG_EXPIRED_LOOKBACK
    if (preserve) {
        f->stress = cast(RELVAL*, malloc(sizeof(RELVAL)));
        memcpy(f->stress, *opt_lookback, sizeof(RELVAL));
        lookback = f->stress;
    }
  #endif

    return lookback;
}

#define Fetch_First_In_Feed(feed) \
    Fetch_Next_In_Feed_Core((feed), false)  // !!! not used at time of writing

inline static const RELVAL *Fetch_Next_In_Feed(  // adds not-end checking
    REBFED *feed,
    bool preserve
){
    assert(KIND3Q_BYTE_UNCHECKED(feed->value) != REB_0_END);
        // ^-- faster than NOT_END()
    return Fetch_Next_In_Feed_Core(feed, preserve);
}


// Most calls to Fetch_Next_In_Frame() are no longer interested in the
// cell backing the pointer that used to be in f->value (this is enforced
// by a rigorous test in DEBUG_EXPIRED_LOOKBACK).  Special care must be
// taken when one is interested in that data, because it may have to be
// moved.  So current can be returned from Fetch_Next_In_Frame_Core().

#define Lookback_While_Fetching_Next(f) \
    Fetch_Next_In_Feed(FRM(f)->feed, true)

#define Fetch_Next_Forget_Lookback(f) \
    ((void)Fetch_Next_In_Feed(FRM(f)->feed, false))


// This code is shared by Literal_Next_In_Feed(), and used without a feed
// advancement in the inert branch of the evaluator.  So for something like
// `loop 2 [append [] 10]`, the steps are:
//
//    1. loop defines its body parameter as <const>
//    2. When LOOP runs Do_Any_Array_At_Throws() on the const ARG(body), the
//       frame gets FEED_FLAG_CONST due to the CELL_FLAG_CONST.
//    3. The argument to append is handled by the inert processing branch
//       which moves the value here.  If the block wasn't made explicitly
//       mutable (e.g. with MUTABLE) it takes the flag from the feed.
//
inline static void Inertly_Derelativize_Inheriting_Const(
    REBVAL *out,
    const RELVAL *v,
    REBFED *feed
){
    Derelativize(out, v, FEED_SPECIFIER(feed));
    SET_CELL_FLAG(out, UNEVALUATED);
    if (not GET_CELL_FLAG(v, EXPLICITLY_MUTABLE))
        out->header.bits |= (feed->flags.bits & FEED_FLAG_CONST);
}

inline static void Literal_Next_In_Feed(REBVAL *out, struct Reb_Feed *feed) {
    Inertly_Derelativize_Inheriting_Const(out, feed->value, feed);
    (void)(Fetch_Next_In_Feed(feed, false));
}


inline static REBFED* Alloc_Feed(void) {
    REBFED* feed = cast(REBFED*, Alloc_Node(FED_POOL));
  #ifdef DEBUG_COUNT_TICKS
    feed->tick = TG_Tick;
  #endif

    Init_Unreadable_Void(Prep_Cell(&feed->fetched));
    Init_Unreadable_Void(Prep_Cell(&feed->lookback));

    REBSER *s = SER(FEED_SINGULAR(feed));
    s->header.bits = NODE_FLAG_NODE
                        | SERIES_FLAG_8_IS_TRUE;
    s->info.bits = Endlike_Header(
        FLAG_WIDE_BYTE_OR_0(0)  // implicit termination
            | FLAG_LEN_BYTE_OR_255(0)
    );
    Prep_Cell(FEED_SINGLE(feed));
    FEED_SPLICE(feed) = nullptr;

    return feed;
}

inline static void Free_Feed(REBFED *feed)
  { Free_Node(FED_POOL, cast(REBNOD*, feed)); }


// It is more pleasant to have a uniform way of speaking of frames by pointer,
// so this macro sets that up for you, the same way DECLARE_LOCAL does.  The
// optimizer should eliminate the extra pointer.
//
// Just to simplify matters, the frame cell is set to a bit pattern the GC
// will accept.  It would need stack preparation anyway, and this simplifies
// the invariant so if a recycle happens before Eval_Core() gets to its
// body, it's always set to something.  Using an unreadable void means we
// signal to users of the frame that they can't be assured of any particular
// value between evaluations; it's not cleared.
//

inline static void Prep_Array_Feed(
    REBFED *feed,
    option(const RELVAL*) first,
    const REBARR *array,
    REBLEN index,
    REBSPC *specifier,
    REBFLGS flags
){
    feed->vaptr = nullptr;
    feed->packed = nullptr;
    feed->flags.bits = flags;
    if (first) {
        feed->value = unwrap(first);
        Init_Any_Array_At_Core(
            FEED_SINGLE(feed), REB_BLOCK, array, index, specifier
        );
        FEED_PENDING(feed) = STABLE(ARR_AT(array, index));
        assert(KIND3Q_BYTE_UNCHECKED(feed->value) != REB_0_END);
            // ^-- faster than NOT_END()
    }
    else {
        feed->value = STABLE(ARR_AT(array, index));
        Init_Any_Array_At_Core(
            FEED_SINGLE(feed), REB_BLOCK, array, index + 1, specifier
        );
        FEED_PENDING(feed) = feed->value + 1;
    }

    feed->gotten = nullptr;
    if (IS_END(feed->value))
        TRASH_POINTER_IF_DEBUG(FEED_PENDING(feed));
    else
        assert(READABLE(feed->value, __FILE__, __LINE__));
}

#define DECLARE_ARRAY_FEED(name,array,index,specifier) \
    REBFED *name = Alloc_Feed(); \
    Prep_Array_Feed(name, \
        nullptr, (array), (index), (specifier), FEED_MASK_DEFAULT \
    );

inline static void Prep_Va_Feed(
    struct Reb_Feed *feed,
    const void *p,
    option(va_list*) vaptr,
    REBFLGS flags
){
    // We want to initialize with something that will give back SPECIFIED.
    // It must therefore be bindable.  Try a COMMA!
    //
    Init_Comma(FEED_SINGLE(feed));

    feed->flags.bits = flags;
    if (not vaptr) {  // `p` should be treated as a packed void* array
        feed->vaptr = nullptr;
        feed->packed = cast(const void* const*, p);
        p = *feed->packed++;
    }
    else {
        feed->vaptr = vaptr;
        feed->packed = nullptr;
    }
    FEED_PENDING(feed) = END_NODE;  // signal next fetch comes from va_list
    Detect_Feed_Pointer_Maybe_Fetch(feed, p, false);

    feed->gotten = nullptr;
    assert(IS_END(feed->value) or READABLE(feed->value, __FILE__, __LINE__));
}

// The flags is passed in by the macro here by default, because it does a
// fetch as part of the initialization from the `first`...and if you want
// FLAG_QUOTING_BYTE() to take effect, it must be passed in up front.
//
#define DECLARE_VA_FEED(name,p,vaptr,flags) \
    REBFED *name = Alloc_Feed(); \
    Prep_Va_Feed(name, (p), (vaptr), (flags)); \

inline static void Prep_Any_Array_Feed(
    REBFED *feed,
    unstable const RELVAL *any_array,  // array is extracted and HOLD put on
    REBSPC *specifier,
    REBFLGS parent_flags  // only reads FEED_FLAG_CONST out of this
){
    // Note that `CELL_FLAG_CONST == FEED_FLAG_CONST`
    //
    REBFLGS flags;
    if (GET_CELL_FLAG(any_array, EXPLICITLY_MUTABLE))
        flags = FEED_MASK_DEFAULT;  // override const from parent frame
    else
        flags = FEED_MASK_DEFAULT
            | (parent_flags & FEED_FLAG_CONST)  // inherit
            | (any_array->header.bits & CELL_FLAG_CONST);  // heed

    Prep_Array_Feed(
        feed,
        nullptr,  // `first` = nullptr, don't inject arbitrary 1st element
        VAL_ARRAY(any_array),
        VAL_INDEX(any_array),
        Derive_Specifier(specifier, any_array),
        flags
    );
}

#define DECLARE_FEED_AT_CORE(name,any_array,specifier) \
    REBFED *name = Alloc_Feed(); \
    Prep_Any_Array_Feed(name, \
        (any_array), (specifier), FS_TOP->feed->flags.bits \
    );

#define DECLARE_FEED_AT(name,any_array) \
    DECLARE_FEED_AT_CORE(name, (any_array), SPECIFIED)
