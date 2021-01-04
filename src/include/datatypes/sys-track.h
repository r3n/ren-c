//
//  File: %sys-track.h
//  Summary: "*VERY USEFUL* Debug Tracking Capabilities for Cell Payloads"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012-2020 Ren-C Open Source Contributors
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
// `Reb_Track_Payload` is the value payload in debug builds for any REBVAL
// whose VAL_TYPE() doesn't need any information beyond the header.  This
// offers a chance to inject some information into the payload to help
// know where the value originated.  It is used by NULL(ed) cells, VOID!,
// BLANK!, and LOGIC!...as well as "trashed" cells.
//
// View this information in the debugging watchlist under the `track` union
// member of a value's payload.  It is also reported by panic().
//
// In addition to the file and line number where the assignment was made,
// the "tick count" of the DO loop is also saved.  This means that it can
// be possible in a repro case to find out which evaluation step produced
// the value--and at what place in the source.  Repro cases can be set to
// break on that tick count, if it is deterministic.
//
// If tracking information is desired for *all* cell types--including those
// that use their payload bits--that means the cell size has to be increased.
// See DEBUG_TRACK_EXTEND_CELLS for this setting, which can be extremely
// useful in tougher debugging cases.
//
// See notes on ZERO_UNUSED_CELL_FIELDS below for why release builds pay the
// cost of initializing unused fields to null, vs. leaving them random.
//

#if defined(DEBUG_TRACK_CELLS)

  #if defined(DEBUG_TRACK_EXTEND_CELLS)  // assume DEBUG_COUNT_TICKS

    #define TOUCH_CELL(c) \
        ((c)->touch = TG_Tick)

    inline static RELVAL *Track_Cell_If_Debug(
        RELVAL *v,
        const char *file,
        int line
    ){
        v->track.file = file;
        v->track.line = line;
        v->extra.tick = v->tick = TG_Tick;
        v->touch = 0;
        return v;
    }

    #define TRACK_CELL_IF_DEBUG(v) \
        Track_Cell_If_Debug((v), __FILE__, __LINE__)

    #define TRACK_CELL_IF_EXTENDED_DEBUG TRACK_CELL_IF_DEBUG

  #elif defined(DEBUG_COUNT_TICKS)

    inline static RELVAL *Track_Cell_If_Debug(
        RELVAL *v,
        const char *file,
        int line
    ){
        v->extra.tick = TG_Tick;
        PAYLOAD(Track, v).file = file;
        PAYLOAD(Track, v).line = line;
        return v;
    }

    #define TRACK_CELL_IF_DEBUG(v) \
        Track_Cell_If_Debug((v), __FILE__, __LINE__)

    #define TRACK_CELL_IF_EXTENDED_DEBUG(v) (v)

  #else  // not counting ticks, and not using extended cell format

    inline static RELVAL *Track_Cell_If_Debug(
        RELVAL *v,
        const char *file,
        int line
    ){
        v->extra.tick = 1;
        PAYLOAD(Track, v).file = file;
        PAYLOAD(Track, v).line = line;
        return v;
    }

    #define TRACK_CELL_IF_DEBUG(v) \
        Track_Cell_If_Debug((v), __FILE__, __LINE__)

    #define TRACK_CELL_IF_EXTENDED_DEBUG(v) (v)

  #endif

#else

    // While debug builds fill the ->extra and ->payload with potentially
    // useful information, it would seem that cells like REB_BLANK which
    // don't use them could just leave them uninitialized...saving time on
    // the assignments.
    //
    // Unfortunately, this is a technically gray area in C.  If you try to
    // copy the memory of that cell (as cells are often copied), it might be a
    // "trap representation".  Reading such representations to copy them...
    // even if not interpreted... is undefined behavior:
    //
    // https://stackoverflow.com/q/60112841
    // https://stackoverflow.com/q/33393569/
    //
    // Odds are it would still work fine if you didn't zero them.  However,
    // compilers will warn you--especially at higher optimization levels--if
    // they notice uninitialized values being used in copies.  This is a bad
    // warning to turn off, because it often points out defective code.
    //
    // So to play it safe and make use of the warnings, fields are zeroed out.
    // But it's set up as its own independent flag, so that someone looking
    // to squeak out a tiny bit more optimization could turn this off in a
    // release build.  It would save on a few null assignments.
    //
    #if !defined(DEBUG_TRACK_EXTEND_CELLS)
        #define ZERO_UNUSED_CELL_FIELDS
    #endif

    #define TRACK_CELL_IF_DEBUG(v) (v)
    #define TRACK_CELL_IF_EXTENDED_DEBUG(v) (v)

#endif
