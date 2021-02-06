//
//  File: %sys-rebctx.h
//  Summary: "context! defs BEFORE %tmp-internals.h"
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
// See %sys-action.h for information about the workings of REBACT and CONTEXT!
// This file just defines basic structures and flags.
//


#define CELL_MASK_CONTEXT \
    (CELL_FLAG_FIRST_IS_NODE  /* varlist */ \
        | CELL_FLAG_SECOND_IS_NODE  /* phase (for FRAME!) */)



// A context's varlist is always allocated dynamically, in order to speed
// up variable access--no need to test USED_BYTE_OR_255 for 255.
//
// !!! Ideally this would carry a flag to tell a GC "shrinking" process not
// to reclaim the dynamic memory to make a singular cell...but that flag
// can't be SERIES_FLAG_FIXED_SIZE, because most varlists can expand.
//
#define SERIES_MASK_VARLIST \
    (NODE_FLAG_NODE | SERIES_FLAG_DYNAMIC \
        | FLAG_FLAVOR(VARLIST) \
        | SERIES_FLAG_LINK_NODE_NEEDS_MARK  /* keysource */ \
        | SERIES_FLAG_MISC_NODE_NEEDS_MARK  /* meta */)

#define SERIES_MASK_KEYLIST \
    (NODE_FLAG_NODE  /* NOT always dynamic */ \
        | FLAG_FLAVOR(KEYLIST) \
        | SERIES_FLAG_LINK_NODE_NEEDS_MARK  /* ancestor */ )


#ifdef CPLUSPLUS_11
    struct Reb_Context : public Reb_Node {
        struct Reb_Series_Base varlist;  // keylist in ->link.keysource
    };
#else
    struct Reb_Context {
        struct Reb_Series varlist;
    };
#endif

#define CTX_VARLIST(c) \
    cast(REBARR*, &(c)->varlist)


#if !defined(DEBUG_CHECK_CASTS)

    #define CTX(p) \
        m_cast(REBCTX*, (const REBCTX*)(p))  // don't check const in C or C++

#else

    template <
        typename T,
        typename T0 = typename std::remove_const<T>::type,
        typename C = typename std::conditional<
            std::is_const<T>::value,  // boolean
            const REBCTX,  // true branch
            REBCTX  // false branch
        >::type
    >
    inline static C *CTX(T *p) {
        static_assert(
            std::is_same<T0, void>::value
                or std::is_same<T0, REBNOD>::value
                or std::is_same<T0, REBSER>::value
                or std::is_same<T0, REBARR>::value,
            "CTX() works on [void* REBNOD* REBSER* REBARR*]"
        );
        if (not p)
            return nullptr;

        if ((reinterpret_cast<const REBSER*>(p)->leader.bits & (
            SERIES_MASK_VARLIST
                | NODE_FLAG_FREE
                | NODE_FLAG_CELL
                | FLAG_FLAVOR_BYTE(255)
                | ARRAY_FLAG_HAS_FILE_LINE_UNMASKED
        )) !=
            SERIES_MASK_VARLIST
        ){
            panic (p);
        }

        return reinterpret_cast<C*>(p);
    }

#endif
