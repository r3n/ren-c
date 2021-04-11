//
//  File: %c-word.c
//  Summary: "symbol table and word related functions"
//  Section: core
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
// In R3-Alpha, words were not garbage collected, and their UTF-8 data was
// kept in a separate table from the REBSERs.  In Ren-C, words use REBSERs,
// and are merely *indexed* by hashes of their canon forms via an external
// table.  This table grows and shrinks as canons are added and removed.
//

#include "sys-core.h"

#define WORD_TABLE_SIZE 1024  // initial size in words


//
// Prime numbers used for hash table sizes. Divide by 2 for
// number of words that can be held in the symbol table.
//
static REBLEN const Primes[] =
{
    7,
    13,
    31,
    61,
    127,
    251,
    509,
    1021,
    2039,
    4093,
    8191,
    16381,
    32749,
    65521,
    131071,
    262139,
    524287,
    1048573,
    2097143,
    4194301,
    8388593,
    16777213,
    33554393,
    67108859,
    134217689,
    268435399,
    536870909,
    1073741789,
    2147483647,
    0xFFFFFFFB, // 4294967291 = 2^32 - 5 (C89)
    0
// see https://primes.utm.edu/lists/2small/0bit.html
};


//
//  Try_Get_Hash_Prime: C
//
// Given a value, return a prime number that is larger or equal.
//
REBINT Try_Get_Hash_Prime(REBLEN minimum)
{
    REBINT n = 0;
    while (minimum > Primes[n]) {
        ++n;
        if (Primes[n] == 0)
            return 0;
    }

    return Primes[n];
}


//
//  Get_Hash_Prime_May_Fail: C
//
REBINT Get_Hash_Prime_May_Fail(REBLEN minimum)
{
    REBINT prime = Try_Get_Hash_Prime(minimum);
    if (prime == 0) {  // larger than hash prime table
        DECLARE_LOCAL (temp);
        Init_Integer(temp, minimum);
        fail (Error_Size_Limit_Raw(temp));
    }
    return prime;
}


// Removals from linear probing lists can be complex, because the same
// overflow slot may be visited through different initial hashes:
//
// http://stackoverflow.com/a/279812/211160
//
// Since it's not enough to simply NULL out the spot when an interned string
// is GC'd, a special pointer signaling "deletedness" is used.  It does not
// cause a linear probe to terminate, but it is reused on insertions.
//
static REBSTR PG_Deleted_Symbol;
#define DELETED_SYMBOL &PG_Deleted_Symbol


//
//  Expand_Word_Table: C
//
// Expand the hash table part of the word_table by allocating
// the next larger table size and rehashing all the words of
// the current table.  Free the old hash array.
//
static void Expand_Word_Table(void)
{
    // The only full list of symbol words available is the old hash table.
    // Hold onto it while creating the new hash table.

    REBLEN old_num_slots = SER_USED(PG_Symbols_By_Hash);
    REBSTR* *old_symbols_by_hash = SER_HEAD(REBSTR*, PG_Symbols_By_Hash);

    REBLEN num_slots = Get_Hash_Prime_May_Fail(old_num_slots + 1);
    assert(SER_WIDE(PG_Symbols_By_Hash) == sizeof(REBSTR*));

    REBSER *ser = Make_Series(
        num_slots, FLAG_FLAVOR(CANONTABLE) | SERIES_FLAG_POWER_OF_2
    );
    Clear_Series(ser);
    SET_SERIES_LEN(ser, num_slots);

    // Rehash all the symbols:

    REBSTR **new_symbols_by_hash = SER_HEAD(REBSTR*, ser);

    REBLEN old_slot;
    for (old_slot = 0; old_slot != old_num_slots; ++old_slot) {
        REBSTR *symbol = old_symbols_by_hash[old_slot];
        if (not symbol)
            continue;

        if (symbol == DELETED_SYMBOL) {  // clean out deleted symbol entries
            --PG_Num_Symbol_Slots_In_Use;
          #if !defined(NDEBUG)
            --PG_Num_Symbol_Deleteds;  // keep track for shutdown assert
          #endif
            continue;
        }

        REBLEN skip;
        REBLEN slot = First_Hash_Candidate_Slot(
            &skip,
            Hash_String(symbol),
            num_slots
        );

        while (new_symbols_by_hash[slot]) {  // skip occupied slots
            slot += skip;
            if (slot >= num_slots)
                slot -= num_slots;
        }
        new_symbols_by_hash[slot] = symbol;
    }

    Free_Unmanaged_Series(PG_Symbols_By_Hash);
    PG_Symbols_By_Hash = ser;
}


//
//  Intern_UTF8_Managed: C
//
// Makes only one copy of each distinct character string:
//
// https://en.wikipedia.org/wiki/String_interning
//
// Interned UTF8 strings are stored as series, and are implicitly managed
// by the GC (because they are shared).
//
// Interning is case-sensitive, but a "synonym" linkage is established between
// instances that are just differently upper-or-lower-"cased".  They agree on
// one "canon" interning to use for fast case-insensitive compares.  If that
// canon form is GC'd, the agreed upon canon for the group will change.
//
const REBSYM *Intern_UTF8_Managed(const REBYTE *utf8, size_t size)
{
    // The hashing technique used is called "linear probing":
    //
    // https://en.wikipedia.org/wiki/Linear_probing
    //
    // For the hash search to be guaranteed to terminate, the table must be
    // large enough that we are able to find a NULL if there's a miss.  (It's
    // actually kept larger than that, but to be on the right side of theory,
    // the table is always checked for expansion needs *before* the search.)
    //
    REBLEN num_slots = SER_USED(PG_Symbols_By_Hash);
    if (PG_Num_Symbol_Slots_In_Use > num_slots / 2) {
        Expand_Word_Table();
        num_slots = SER_USED(PG_Symbols_By_Hash);  // got larger
    }

    REBSYM* *symbols_by_hash = SER_HEAD(REBSYM*, PG_Symbols_By_Hash);

    REBLEN skip; // how many slots to skip when occupied candidates found
    REBLEN slot = First_Hash_Candidate_Slot(
        &skip,
        Hash_Scan_UTF8_Caseless_May_Fail(utf8, size),
        num_slots
    );

    // The hash table only indexes the canon form of each spelling.  So when
    // testing a slot to see if it's a match (or a collision that needs to
    // be skipped to try again) the search uses a comparison that is
    // case-insensitive...but reports if synonyms via > 0 results.
    //
    REBSYM *synonym = nullptr;
    REBSYM **deleted_slot = nullptr;
    REBSYM* symbol;
    while ((symbol = symbols_by_hash[slot])) {
        if (symbol == DELETED_SYMBOL) {
            deleted_slot = &symbols_by_hash[slot];
            goto next_candidate_slot;
        }

      blockscope {
        REBINT cmp = Compare_UTF8(STR_HEAD(symbol), utf8, size);
        if (cmp == 0)
            return symbol;  // was a case-sensitive match
        if (cmp < 0)
            goto next_candidate_slot;  // wasn't an alternate casing

        // The > 0 result means that the canon word that was found is an
        // alternate casing ("synonym") for the string we're interning.  The
        // synonyms are attached to the canon form with a circular list.
        //
        synonym = symbol;  // save for linking into synonyms list
        goto next_candidate_slot;
      }

        goto new_interning;  // no synonym matched, make new synonym for canon

      next_candidate_slot:  // https://en.wikipedia.org/wiki/Linear_probing

        slot += skip;
        if (slot >= num_slots)
            slot -= num_slots;
    }

  new_interning: {

    REBBIN *s = BIN(Make_Series(
        size + 1,  // if small, fits in a REBSER node (w/no data allocation)
        FLAG_FLAVOR(SYMBOL) | SERIES_FLAG_FIXED_SIZE
    ));

    // The incoming string isn't always null terminated, e.g. if you are
    // interning `foo` in `foo: bar + 1` it would be colon-terminated.
    //
    memcpy(BIN_HEAD(s), utf8, size);
    TERM_BIN_LEN(s, size);

    // The UTF-8 series can be aliased with AS to become an ANY-STRING! or a
    // BINARY!.  If it is, then it should not be modified.
    //
    Freeze_Series(s); 

    if (not synonym) {
        mutable_LINK(Synonym, s) = SYM(s);  // 1-item in circular list

        // leave header.bits as 0 for SYM_0 as answer to VAL_WORD_ID()
        // Startup_Symbols() tags values from %words.r after the fact.
        //
        // Words that aren't in the bootup %words.r list don't have integer
        // IDs defined that can be used in compiled C switch() cases (e.g.
        // SYM_ANY, SYM_INTEGER_X, etc.)  So if we didn't find a pre-existing
        // synonym, and none is added, it will remain at 0.
        //
        // !!! It is proposed that a pre-published dictionary of small words
        // could be agreed on, and then extensions using those words could
        // request the numbers for those words.  Inconsistent requests that
        // didn't follow the published list could cause an error.  This would
        // give more integer values without more strings in the core.
        //
        assert(SECOND_UINT16(s->info) == 0);
    }
    else {
        // This is a synonym for an existing canon.  Link it into the synonyms
        // circularly linked list, and direct link the canon form.
        //
        mutable_LINK(Synonym, s) = LINK(Synonym, synonym);
        mutable_LINK(Synonym, synonym) = SYM(s);

        // If the canon form had a SYM_XXX for quick comparison of %words.r
        // words in C switch statements, the synonym inherits that number.
        //
        assert(SECOND_UINT16(s->info) == 0);
        SET_SECOND_UINT16(s->info, ID_OF_SYMBOL(synonym));
    }

    // Symbols use their MISC() to hold binding information.  Long term, it
    // may become a design that lets multiple binds run at once.  So the slot
    // could hold an atomic pointer that would "pop out" to a structure.
    // But for the moment only one bind runs at a time, and it's randomized to
    // keep its information in high bits or low bits as a poor-man's demo that
    // there is an infrastructure in place for sharing (start with 2, grow to
    // N eventually).
    //
    s->misc.bind_index.high = 0;
    s->misc.bind_index.low = 0;

    if (deleted_slot) {
        *deleted_slot = SYM(s);  // reuse the deleted slot
      #if !defined(NDEBUG)
        --PG_Num_Symbol_Deleteds;  // note slot usage count stays constant
      #endif
    }
    else {
        symbols_by_hash[slot] = SYM(s);
        ++PG_Num_Symbol_Slots_In_Use;
    }

    // Created series must be managed, because if they were not there could
    // be no clear contract on the return result--as it wouldn't be possible
    // to know if a shared instance had been managed by someone else or not.
    //
    return SYM(Manage_Series(s));
  }
}


//
//  Intern_Any_String_Managed: C
//
// The main use of interning ANY-STRING! is FILE! for ARRAY_FLAG_FILE_LINE.
// It's important to make a copy, because you would not want the change of
// `file` to affect the filename references in already loaded sources:
//
//     file: copy %test
//     x: transcode/file data1 file
//     append file "-2"  ; shouldn't change FILE OF X answer
//     y: transcode/file data2 file
//
// So mutable series shouldn't be used directly.  Reusing the string interning
// mechanics also cuts down on storage of redundant data (though it needs to
// allow spaces).
//
// !!! With UTF-8 Everywhere, could locked strings be used here?  Should all
// locked strings become interned, and forward pointers to the old series in
// the background to the interned version?
//
const REBSTR *Intern_Any_String_Managed(const RELVAL *v) {
    REBSIZ utf8_size;
    REBCHR(const*) utf8 = VAL_UTF8_SIZE_AT(&utf8_size, v);
    return Intern_UTF8_Managed(utf8, utf8_size);
}


//
//  GC_Kill_Interning: C
//
// Unlink this spelling out of the circularly linked list of synonyms.
// Further, if it happens to be canon, we need to re-point everything in the
// chain to a new entry.  Choose the synonym as a new canon if so.
//
void GC_Kill_Interning(REBSTR *intern)
{
    REBSYM *synonym = LINK(Synonym, intern);

    // Note synonym and intern may be the same here.
    //
    REBSYM *temp = synonym;
    while (LINK(Synonym, temp) != intern)
        temp = LINK(Synonym, temp);
    mutable_LINK(Synonym, temp) = synonym;  // cut the intern out (or no-op)

    assert(intern->misc.bind_index.high == 0);  // shouldn't GC during binds?
    assert(intern->misc.bind_index.low == 0);

    REBLEN num_slots = SER_USED(PG_Symbols_By_Hash);
    REBSTR* *symbols_by_hash = SER_HEAD(REBSTR*, PG_Symbols_By_Hash);

    REBLEN skip;
    REBLEN slot = First_Hash_Candidate_Slot(
        &skip,
        Hash_String(intern),
        num_slots
    );

    // We *will* find the canon form in the hash table.
    //
    while (symbols_by_hash[slot] != intern) {
        slot += skip;
        if (slot >= num_slots)
            slot -= num_slots;
    }

    // This canon form must be removed from the hash table.  Ripple the
    // collision slots back until a NULL is found, to reduce search times.
    //
    REBLEN previous_slot = slot;
    while (symbols_by_hash[slot]) {
        slot += skip;
        if (slot >= num_slots)
            slot -= num_slots;
        symbols_by_hash[previous_slot] = symbols_by_hash[slot];
    }

    // Signal that the hash slot is "deleted" via a special pointer.
    // See notes on DELETED_SLOT for why the final slot in the collision
    // chain can't just be left NULL:
    //
    // http://stackoverflow.com/a/279812/211160
    //
    symbols_by_hash[previous_slot] = DELETED_SYMBOL;

  #if !defined(NDEBUG)
    ++PG_Num_Symbol_Deleteds;  // total use same (PG_Num_Symbols_Or_Deleteds)
  #endif
}


//
//  Startup_Interning: C
//
// Get the engine ready to do Intern_UTF8_Managed(), which is required to
// get REBSTR* pointers generated during a scan of ANY-WORD!s.  Words of the
// same spelling currently look up and share the same REBSTR*, this process
// is referred to as "string interning":
//
// https://en.wikipedia.org/wiki/String_interning
//
void Startup_Interning(void)
{
    PG_Num_Symbol_Slots_In_Use = 0;
  #if !defined(NDEBUG)
    PG_Num_Symbol_Deleteds = 0;
  #endif

    // Start hash table out at a fixed size.  When collisions occur, it
    // causes a skipping pattern that continues until it finds the desired
    // slot.  The method is known as linear probing:
    //
    // https://en.wikipedia.org/wiki/Linear_probing
    //
    // It must always be at least as big as the total number of words, in order
    // for it to uniquely be able to locate each symbol pointer.  But to
    // reduce long probing chains, it should be significantly larger than that.
    // R3-Alpha used a heuristic of 4 times as big as the number of words.

    REBLEN n;
  #if defined(NDEBUG)
    n = Get_Hash_Prime_May_Fail(WORD_TABLE_SIZE * 4);  // *4 reduces rehashing
  #else
    n = 1; // forces exercise of rehashing logic in debug build
  #endif

    PG_Symbols_By_Hash = Make_Series(
        n, FLAG_FLAVOR(CANONTABLE) | SERIES_FLAG_POWER_OF_2
    );
    Clear_Series(PG_Symbols_By_Hash);  // all slots start as nullptr
    SET_SERIES_LEN(PG_Symbols_By_Hash, n);
}


//
//  Startup_Early_Symbols: C
//
// It's very desirable to have `/`, `/foo`, `/foo/`, `/foo/(bar)` etc. be
// instances of the same datatype of PATH!.  In this scheme, `/` would act
// like a "root path" and be achieved with `to path! [_ _]`.
//
// But with limited ASCII symbols, there is strong demand for `/` to be able
// to act like division in evaluative contexts, or to be overrideable for
// other things in a way not too dissimilar from `+`.
//
// The compromise used is to make `/` be a cell whose VAL_TYPE() is REB_PATH,
// but whose CELL_KIND() is REB_WORD with the special spelling `-1-SLASH-`.
// Binding mechanics and evaluator behavior are based on this unusual name.
// But when inspected by the user, it appears to be a PATH! with 2 blanks.
//
// This duality is imperfect, as any routine with semantics like COLLECT-WORDS
// would have to specifically account for it, or just see an empty path.
// But it is intended to give some ability to configure the behavior easily.
//
// The trick which allows the `/` to be a 2-element PATH! and yet act like a
// WORD! when used in evaluative contexts requires that word's spelling to be
// available during scanning.  But scanning is what loads the %words.r symbol
// list!  Break the Catch-22 by manually interning the symbol used.
//
// (Same issue applies to the symbol in ~unreadable~ in release builds, used
// e.g. by the data stack initialization.  In debug builds NULL is used to
// detect the errors on reads.)
//
void Startup_Early_Symbols(void)
{
    const char *slash1 = "-slash-1-";
    assert(PG_Slash_1_Canon == nullptr);
    PG_Slash_1_Canon = Intern_UTF8_Managed(cb_cast(slash1), strsize(slash1));

    const char *dot1 = "-dot-1-";
    assert(PG_Dot_1_Canon == nullptr);
    PG_Dot_1_Canon = Intern_UTF8_Managed(cb_cast(dot1), strsize(dot1));

    const char *unreadable = "unreadable";
    assert(PG_Unreadable_Canon == nullptr);
    PG_Unreadable_Canon = Intern_UTF8_Managed(
        cb_cast(unreadable),
        strsize(unreadable)
    );
}


//
//  Startup_Symbols: C
//
// By this point in the boot, the canon words have already been interned for
// everything in %words.r.
//
// This goes through the name series for %words.r words and tags them with
// SYM_XXX constants.  This allows the small number to be quickly extracted to
// use with VAL_WORD_ID() in C switch statements.  These are the only words
// that have fixed symbol numbers--others are only managed and compared
// through their pointers.
//
// It also creates a table for mapping from SYM_XXX => REBSTR series.  This
// is used e.g. by Canon(SYM_XXX) to get the string name for a symbol.
//
void Startup_Symbols(REBARR *words)
{
    assert(PG_Symbol_Canons == nullptr);
    PG_Symbol_Canons = Make_Series(
        1 + ARR_LEN(words),  // 1 + => extra trash at head for SYM_0
        FLAG_FLAVOR(COMMONWORDS)
            | SERIES_FLAG_FIXED_SIZE  // can't ever add more SYM_XXX lookups
    );

    // All words that not in %words.r will get back VAL_WORD_ID(w) == SYM_0
    // Hence, SYM_0 cannot be canonized.  Allowing Canon(SYM_0) to return NULL
    // and try and use that meaningfully is too risky, so it is simply
    // prohibited to canonize SYM_0, and trash the REBSTR* in the [0] slot.
    //
    SYMID sym = SYM_0;
    TRASH_POINTER_IF_DEBUG(
        *SER_AT(REBSTR*, PG_Symbol_Canons, cast(REBLEN, sym))
    );

    const RELVAL *tail = ARR_TAIL(words);
    const RELVAL *word = ARR_HEAD(words);
    for (; word != tail; ++word) {
        assert(IS_WORD(word));  // real word, not fake (e.g. `/` as -slash-0-)
        REBSYM *canon = m_cast(REBSYM*, VAL_WORD_SYMBOL(word));

        sym = cast(SYMID, cast(REBLEN, sym) + 1);
        *SER_AT(REBSTR*, PG_Symbol_Canons, cast(REBLEN, sym)) = canon;

        if (sym == SYM__SLASH_1_)
            assert(canon == PG_Slash_1_Canon);  // make sure it lined up!
        else if (sym == SYM__DOT_1_)
            assert(canon == PG_Dot_1_Canon);
        else if (sym == SYM_UNREADABLE)
            assert(canon == PG_Unreadable_Canon);

        // More code was loaded than just the word list, and it might have
        // included alternate-case forms of the %words.r words.  Walk any
        // aliases and make sure they have the header bits too.

        REBSYM *name = canon;
        do {
            // Symbol series store symbol number in the header's 2nd uint16_t.
            // Could probably use less than 16 bits, but 8 is insufficient.
            // (length %words.r > 256)
            //
            assert(SECOND_UINT16(name->info) == 0);
            SET_SECOND_UINT16(name->info, sym);
            assert(Same_Nonzero_Symid(ID_OF_SYMBOL(name), sym));

            name = LINK(Synonym, name);
        } while (name != canon); // circularly linked list, stop on a cycle
    }

    *SER_AT(REBSTR*, PG_Symbol_Canons, cast(REBLEN, sym)) = NULL; // terminate

    SET_SERIES_USED(PG_Symbol_Canons, 1 + cast(REBLEN, sym));
    assert(SER_USED(PG_Symbol_Canons) == 1 + ARR_LEN(words));

    // Do some sanity checks.  !!! Fairly critical, is debug-only appropriate?

    if (0 != strcmp("blank!", STR_UTF8(Canon(SYM_BLANK_X))))
        panic (Canon(SYM_BLANK_X));

    if (0 != strcmp("true", STR_UTF8(Canon(SYM_TRUE))))
        panic (Canon(SYM_TRUE));

    if (0 != strcmp("open", STR_UTF8(Canon(SYM_OPEN))))
        panic (Canon(SYM_OPEN));

    PG_Bar_Canon = Canon(SYM_BAR);  // used by PARSE for speedup
}


//
//  Shutdown_Symbols: C
//
void Shutdown_Symbols(void)
{
    Free_Unmanaged_Series(PG_Symbol_Canons);
    PG_Symbol_Canons = nullptr;

    PG_Slash_1_Canon = nullptr;
    PG_Dot_1_Canon = nullptr;
    PG_Unreadable_Canon = nullptr;
}


//
//  Shutdown_Interning: C
//
void Shutdown_Interning(void)
{
  #if !defined(NDEBUG)
    if (PG_Num_Symbol_Slots_In_Use - PG_Num_Symbol_Deleteds != 0) {
        //
        // !!! There needs to be a more user-friendly output for this,
        // and to detect if it really was an API problem or something else
        // that needs to be paid attention to in the core.  Right now the
        // two scenarios are conflated into this one panic.
        //
        printf(
            "!!! %d leaked canons found in shutdown\n",
            cast(int, PG_Num_Symbol_Slots_In_Use - PG_Num_Symbol_Deleteds)
        );
        printf("!!! LIKELY rebUnmanage() without a rebRelease() in API\n");

        fflush(stdout);

        REBLEN slot;
        for (slot = 0; slot < SER_USED(PG_Symbols_By_Hash); ++slot) {
            REBSTR *symbol = *SER_AT(REBSTR*, PG_Symbols_By_Hash, slot);
            if (symbol and symbol != DELETED_SYMBOL)
                panic (symbol);
        }
    }
  #endif

    Free_Unmanaged_Series(PG_Symbols_By_Hash);
}
