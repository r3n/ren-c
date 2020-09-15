//
//  File: %sys-binary.h
//  Summary: {Definitions for binary series}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2019 Ren-C Open Source Contributors
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
// A BINARY! value holds a byte-size series.  The bytes may be arbitrary, or
// if the series has SERIES_FLAG_IS_STRING then modifications are constrained
// to only allow valid UTF-8 data.  Such binary "views" are possible due to
// things like the AS operator (`as binary! "abc"`).
//
// R3-Alpha used a binary series to hold the data for BITSET!.  See notes in
// %sys-bitset.h regarding this usage (which has a "negated" bit in the
// MISC() field).
//
//=//// NOTES /////////////////////////////////////////////////////////////=//
//
// * Since strings use MISC() and LINK() for various features, and binaries
//   can be "views" on string series, this means that generally speaking a
//   binary series can't use MISC() and LINK() for its own purposes.  (For
//   the moment, typesets cannot be aliased, so you can't get into a situation
//   like `as text! as binary! make bitset! [...]`)


//=//// BINARY! SERIES ////////////////////////////////////////////////////=//

inline static REBYTE *BIN_AT(const_if_c REBBIN *bin, REBLEN n)
  { return SER_AT(REBYTE, bin, n); }

inline static REBYTE *BIN_HEAD(const_if_c REBBIN *bin)
  { return SER_HEAD(REBYTE, bin); }

inline static REBYTE *BIN_TAIL(const_if_c REBBIN *bin)
  { return SER_TAIL(REBYTE, bin); }

inline static REBYTE *BIN_LAST(const_if_c REBBIN *bin)
  { return SER_LAST(REBYTE, bin); }

#ifdef __cplusplus
    inline static const REBYTE *BIN_AT(const REBBIN *bin, REBLEN n)
      { return SER_AT(const REBYTE, bin, n); }

    inline static const REBYTE *BIN_HEAD(const REBBIN *bin)
      { return SER_HEAD(const REBYTE, bin); }

    inline static const REBYTE *BIN_TAIL(const REBBIN *bin)
      { return SER_TAIL(const REBYTE, bin); }

    inline static const REBYTE *BIN_LAST(const REBBIN *bin)
      { return SER_LAST(const REBYTE, bin); }
#endif

inline static REBLEN BIN_LEN(const REBBIN *s) {
    assert(SER_WIDE(s) == 1);
    return SER_USED(s);
}

inline static void TERM_BIN(REBSER *s) {
    assert(SER_WIDE(s) == 1);
    BIN_HEAD(s)[SER_USED(s)] = 0;
}

inline static void TERM_BIN_LEN(REBSER *s, REBLEN len) {
    assert(SER_WIDE(s) == 1);
    SET_SERIES_USED(s, len);
    BIN_HEAD(s)[len] = 0;
}

// Make a byte series of length 0 with the given capacity.  The length will
// be increased by one in order to allow for a null terminator.  Binaries are
// given enough capacity to have a null terminator in case they are aliased
// as UTF-8 data later, e.g. `as word! binary`, since it would be too late
// to give them that capacity after-the-fact to enable this.
//
inline static REBSER *Make_Binary_Core(REBLEN capacity, REBFLGS flags)
{
    REBSER *bin = Make_Series_Core(capacity + 1, sizeof(REBYTE), flags);
    TERM_SEQUENCE(bin);
    return bin;
}

#define Make_Binary(capacity) \
    Make_Binary_Core(capacity, SERIES_FLAGS_NONE)


//=//// BINARY! VALUES ////////////////////////////////////////////////////=//

#define VAL_BIN_HEAD(v) \
    BIN_HEAD(VAL_SERIES(v))

inline static const REBYTE *VAL_BIN_AT(REBCEL(const*) v) {
    assert(CELL_KIND(v) == REB_BINARY or CELL_KIND(v) == REB_BITSET);
    if (VAL_INDEX(v) > BIN_LEN(VAL_SERIES(v)))
        fail (Error_Past_End_Raw());  // don't give deceptive return pointer
    return BIN_AT(VAL_SERIES(v), VAL_INDEX(v));
}

#define VAL_BIN_AT_ENSURE_MUTABLE(v) \
    m_cast(REBYTE*, VAL_BIN_AT(ENSURE_MUTABLE(v)))

#define VAL_BIN_AT_KNOWN_MUTABLE(v) \
    m_cast(REBYTE*, VAL_BIN_AT(KNOWN_MUTABLE(v)))

#define Init_Binary(out,bin) \
    Init_Any_Series((out), REB_BINARY, (bin))

#define Init_Binary_At(out,bin,offset) \
    Init_Any_Series_At((out), REB_BINARY, (bin), (offset))

inline static const REBBIN *VAL_BINARY(REBCEL(const*) v) {
    assert(CELL_KIND(v) == REB_BINARY);
    return VAL_SERIES(v);
}

#define VAL_BINARY_ENSURE_MUTABLE(v) \
    m_cast(REBBIN*, VAL_BINARY(ENSURE_MUTABLE(v)))

#define VAL_BINARY_KNOWN_MUTABLE(v) \
    m_cast(REBBIN*, VAL_BINARY(KNOWN_MUTABLE(v)))

#define BYTE_BUF TG_Byte_Buf
