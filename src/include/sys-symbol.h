//
//  File: %sys-symbol.h
//  Summary: {Definitions for Symbols and Symbol IDs}
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
// A SYM refers to one of the built-in words and can be used in C switch
// statements.  A canon STR is used to identify everything else.
// 
// R3-Alpha's concept was that all words got persistent integer values, which
// prevented garbage collection.  Ren-C only gives built-in words integer
// values--or SYMs--while others must be compared by pointers to their
// name or canon-name pointers.  A non-built-in symbol will return SYM_0 as
// its symbol, allowing it to fall through to defaults in case statements.
//
// Though it works fine for switch statements, it creates a problem if someone
// writes `VAL_WORD_SYM(a) == VAL_WORD_SYM(b)`, because all non-built-ins
// will appear to be equal.  It's a tricky enough bug to catch to warrant an
// extra check in C++ that disallows comparing SYMs with ==
//


// For a *read-only* REBSTR, circularly linked list of othEr-CaSed string
// forms.  It should be relatively quick to find the canon form on
// average, since many-cased forms are somewhat rare.
//
// Note: String series using this don't have SERIES_FLAG_LINK_NODE_NEEDS_MARK.
// One synonym need not keep another alive, because the process of freeing
// string nodes unlinks them from the list.  (Hence the canon can change!)
//
#define LINK_SYNONYM_NODE(s)    LINK(s).custom.node
#define LINK_SYNONYM(s)         STR(LINK_SYNONYM_NODE(s))


#if defined(NDEBUG) || !defined(CPLUSPLUS_11)
    //
    // Trivial definition for C build or release builds: symbols are just a C
    // enum value and an OPT_REBSYM acts just like a REBSYM.
    //
    typedef enum Reb_Symbol REBSYM;
    typedef enum Reb_Symbol OPT_REBSYM;
#else
    struct REBSYM;

    struct OPT_REBSYM {  // may only be converted to REBSYM, no comparisons
        enum Reb_Symbol n;
        OPT_REBSYM (const REBSYM& sym);
        bool operator==(enum Reb_Symbol other) const
          { return n == other; }
        bool operator!=(enum Reb_Symbol other) const
          { return n != other; }

        bool operator==(OPT_REBSYM &&other) const = delete;
        bool operator!=(OPT_REBSYM &&other) const = delete;

        operator unsigned int() const  // so it works in switch() statements
          { return cast(unsigned int, n); }

        explicit operator enum Reb_Symbol()  // must be an *explicit* cast
          { return n; }
    };

    struct REBSYM {  // acts like a REBOL_Symbol with no OPT_REBSYM compares
        enum Reb_Symbol n;
        REBSYM () {}
        REBSYM (int n) : n (cast(enum Reb_Symbol, n)) {}
        REBSYM (OPT_REBSYM opt_sym) : n (opt_sym.n) {}

        operator unsigned int() const  // so it works in switch() statements
          { return cast(unsigned int, n); }

        explicit operator enum Reb_Symbol() {  // must be an *explicit* cast
            assert(n != SYM_0);
            return n;
        }

        bool operator>=(enum Reb_Symbol other) const {
            assert(other != SYM_0);
            return n >= other;
        }
        bool operator<=(enum Reb_Symbol other) const {
            assert(other != SYM_0);
            return n <= other;
        }
        bool operator>(enum Reb_Symbol other) const {
            assert(other != SYM_0);
            return n > other;
        }
        bool operator<(enum Reb_Symbol other) const {
            assert(other != SYM_0);
            return n < other;
        }
        bool operator==(enum Reb_Symbol other) const
          { return n == other; }
        bool operator!=(enum Reb_Symbol other) const
          { return n != other; }

        bool operator==(REBSYM &other) const = delete;  // may be SYM_0
        void operator!=(REBSYM &other) const = delete;  // ...same
        bool operator==(const OPT_REBSYM &other) const = delete;  // ...same
        void operator!=(const OPT_REBSYM &other) const = delete;  // ...same
    };

    inline OPT_REBSYM::OPT_REBSYM(const REBSYM &sym) : n (sym.n) {}
#endif

inline static bool SAME_SYM_NONZERO(REBSYM a, REBSYM b) {
    assert(a != SYM_0 and b != SYM_0);
    return cast(REBLEN, a) == cast(REBLEN, b);
}

inline static OPT_REBSYM STR_SYMBOL(const REBSTR *s) {
    assert(IS_STR_SYMBOL(s));
    return cast(REBSYM, SECOND_UINT16(s->header));
}

inline static const REBSTR *Canon(REBSYM sym) {
    assert(cast(REBLEN, sym) != 0);
    assert(cast(REBLEN, sym) < SER_USED(PG_Symbol_Canons));  // null if boot!
    return *SER_AT(const REBSTR*, PG_Symbol_Canons, cast(REBLEN, sym));
}

inline static bool SAME_STR(const REBSTR *s1, const REBSTR *s2) {
    assert(IS_STR_SYMBOL(s1));
    assert(IS_STR_SYMBOL(s2));

    const REBSTR *temp = s1;
    do {
        if (temp == s2)
            return true;
    } while ((temp = LINK_SYNONYM(temp)) != s1);

    return false;  // stopped when circularly linked list loops back to self
}
