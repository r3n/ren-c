//
//  File: %sys-map.h
//  Summary: {Definitions for REBMAP}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Ren-C Open Source Contributors
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
// Maps are implemented as a light hashing layer on top of an array.  The
// hash indices are stored in the series node's "misc", while the values are
// retained in pairs as `[key val key val key val ...]`.
//
// When there are too few values to warrant hashing, no hash indices are
// made and the array is searched linearly.  This is indicated by the hashlist
// being NULL.
//
// Though maps are not considered a series in the "ANY-SERIES!" value sense,
// they are implemented using series--and hence are in %sys-series.h, at least
// until a better location for the definition is found.
//
// !!! Should there be a MAP_LEN()?  Current implementation has NONE in
// slots that are unused, so can give a deceptive number.  But so can
// objects with hidden fields, locals in paramlists, etc.
//

#define SERIES_MASK_PAIRLIST \
    (FLAG_FLAVOR(PAIRLIST) \
        | SERIES_FLAG_LINK_NODE_NEEDS_MARK  /* hashlist */)


#ifdef CPLUSPLUS_11
    struct Reb_Map : public Reb_Node {
        struct Reb_Series_Base pairlist;  // hashlist in LINK(Hashlist)
    };
#else
    struct Reb_Map {
        struct Reb_Series pairlist;
    };
#endif


// See LINK() macro for how this is used.
//
#define LINK_Hashlist_TYPE          REBSER*
#define LINK_Hashlist_CAST          SER
#define HAS_LINK_Hashlist           FLAVOR_PAIRLIST

inline static REBARR *MAP_PAIRLIST(const_if_c REBMAP *m) {
    return cast(REBARR*, (&m_cast(REBMAP*, m)->pairlist));
}

#ifdef __cplusplus
    inline static const REBARR *MAP_PAIRLIST(const REBMAP *m)
      { return MAP_PAIRLIST(m_cast(REBMAP*, m)); }
#endif

#define MAP_HASHLIST(m) \
    LINK(Hashlist, MAP_PAIRLIST(m))

#define MAP_HASHES(m) \
    SER_HEAD(MAP_HASHLIST(m))


inline static const REBMAP *VAL_MAP(REBCEL(const*) v) {
    assert(CELL_KIND(v) == REB_MAP);

    REBARR *a = ARR(VAL_NODE1(v));
    if (GET_SERIES_FLAG(a, INACCESSIBLE))
        fail (Error_Series_Data_Freed_Raw());

    return MAP(a);
}

#define VAL_MAP_ENSURE_MUTABLE(v) \
    m_cast(REBMAP*, VAL_MAP(ENSURE_MUTABLE(v)))

#define VAL_MAP_KNOWN_MUTABLE(v) \
    m_cast(REBMAP*, VAL_MAP(KNOWN_MUTABLE(v)))

inline static REBLEN Length_Map(const REBMAP *map)
{
    const RELVAL *tail = ARR_TAIL(MAP_PAIRLIST(map));
    const REBVAL *v = cast(const REBVAL*, ARR_HEAD(MAP_PAIRLIST(map)));

    REBLEN count = 0;
    for (; v != tail; v += 2) {
        if (not IS_NULLED(v + 1))
            ++count;
    }

    return count;
}
