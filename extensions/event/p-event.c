//
//  File: %p-event.c
//  Summary: "event port interface"
//  Section: ports
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
/*
  Basics:

      Ports use requests to control devices.
      Devices do their best, and return when no more is possible.
      Progs call WAIT to check if devices have changed.
      If devices changed, modifies request, and sends event.
      If no devices changed, timeout happens.
      On REBOL side, we scan event queue.
      If we find an event, we call its port/awake function.

      Different cases exist:

      1. wait for time only

      2. wait for ports and time.  Need a master wait list to
         merge with the list provided this function.

      3. wait for windows to close - check each time we process
         a close event.

      4. what to do on console ESCAPE interrupt? Can use catch it?

      5. how dow we relate events back to their ports?

      6. async callbacks
*/

#include "sys-core.h"

#include "reb-event.h"

#define EVENTS_LIMIT 0xFFFF //64k
#define EVENTS_CHUNK 128

//
//  Append_Event: C
//
// Append an event to the end of the current event port queue.
// Return a pointer to the event value.
//
// Note: this function may be called from out of environment,
// so do NOT extend the event queue here. If it does not have
// space, return 0. (Should it overwrite or wrap???)
//
REBVAL *Append_Event(void)
{
    REBVAL *port = Get_System(SYS_PORTS, PORTS_SYSTEM);
    if (!IS_PORT(port)) return 0; // verify it is a port object

    // Get queue block:
    REBVAL *state = VAL_CONTEXT_VAR(port, STD_PORT_STATE);
    if (!IS_BLOCK(state)) return 0;

    // Append to tail if room:
    if (SER_FULL(VAL_SERIES(state))) {
        if (VAL_LEN_HEAD(state) > EVENTS_LIMIT)
            panic (state);

        Extend_Series(VAL_SERIES_KNOWN_MUTABLE(state), EVENTS_CHUNK);
    }

    REBARR *state_array = VAL_ARRAY_KNOWN_MUTABLE(state);
    TERM_ARRAY_LEN(state_array, VAL_LEN_HEAD(state) + 1);
    return Init_Blank(ARR_LAST(state_array));
}


//
//  Find_Last_Event: C
//
// Find the last event in the queue by the model
// Check its type, if it matches, then return the event or NULL
//
// !!! Not currently used.
//
const REBVAL *Find_Last_Event(REBINT model, uint32_t type)
{
    REBVAL *port = Get_System(SYS_PORTS, PORTS_SYSTEM);
    if (!IS_PORT(port)) return NULL; // verify it is a port object

    // Get queue block:
    REBVAL *state = VAL_CONTEXT_VAR(port, STD_PORT_STATE);
    if (!IS_BLOCK(state)) return NULL;

    const RELVAL *value = VAL_ARRAY_TAIL(state) - 1;
    for (; value >= ARR_HEAD(VAL_ARRAY(state)); --value) {
        if (VAL_EVENT_MODEL(value) == model) {
            if (cast(uint32_t, VAL_EVENT_TYPE(value)) == type) {
                return cast(const REBVAL*, value);
            } else {
                return NULL;
            }
        }
    }

    return NULL;
}

//
//  Event_Actor: C
//
// Internal port handler for events.
//
REB_R Event_Actor(REBFRM *frame_, REBVAL *port, const REBVAL *verb)
{
    // Validate and fetch relevant PORT fields:
    //
    REBCTX *ctx = VAL_CONTEXT(port);
    REBVAL *state = CTX_VAR(ctx, STD_PORT_STATE);
    REBVAL *spec = CTX_VAR(ctx, STD_PORT_SPEC);
    if (!IS_OBJECT(spec))
        fail (Error_Invalid_Spec_Raw(spec));

    // Get or setup internal state data:
    //
    if (!IS_BLOCK(state))
        Init_Block(state, Make_Array(EVENTS_CHUNK - 1));

    switch (VAL_WORD_SYM(verb)) {

    case SYM_REFLECT: {
        INCLUDE_PARAMS_OF_REFLECT;

        UNUSED(ARG(value)); // implicit in port
        REBSYM property = VAL_WORD_SYM(ARG(property));
        assert(property != SYM_0);

        switch (property) {
        case SYM_LENGTH:
            return Init_Integer(D_OUT, VAL_LEN_HEAD(state));

        default:
            break;
        }

        break; }

    case SYM_ON_WAKE_UP:
        return Init_Void(D_OUT, SYM_VOID);

    // Normal block actions done on events:
    case SYM_POKE:
        if (not IS_EVENT(D_ARG(3)))
            fail (D_ARG(3));
        goto act_blk;

    case SYM_INSERT:
    case SYM_APPEND:
        if (not IS_EVENT(D_ARG(2)))
            fail (D_ARG(2));
        goto act_blk;

    case SYM_PICK: {
    act_blk:;
        //
        // !!! For performance, this reuses the same frame built for the
        // INSERT/etc. on a PORT! to do an INSERT/etc. on whatever kind of
        // value the state is.  It saves the value of the port, substitutes
        // the state value in the first slot of the frame, and calls the
        // array type dispatcher.  :-/
        //
        DECLARE_LOCAL (save_port);
        Move_Value(save_port, D_ARG(1));
        Move_Value(D_ARG(1), state);

        REB_R r = T_Array(frame_, verb);
        SET_SIGNAL(SIG_EVENT_PORT);
        if (
            VAL_WORD_SYM(verb) == SYM_INSERT
            || VAL_WORD_SYM(verb) == SYM_APPEND
            || VAL_WORD_SYM(verb) == SYM_REMOVE
        ){
            RETURN (save_port);
        }
        return r; }

    case SYM_CLEAR:
        TERM_ARRAY_LEN(VAL_ARRAY_KNOWN_MUTABLE(state), 0);
        CLR_SIGNAL(SIG_EVENT_PORT);
        RETURN (port);

    case SYM_OPEN: {
        INCLUDE_PARAMS_OF_OPEN;

        UNUSED(PAR(spec));

        if (REF(new) or REF(read) or REF(write) or REF(seek) or REF(allow))
            fail (Error_Bad_Refines_Raw());

        REBREQ *req = OS_Make_Devreq(&Dev_Event);

        Req(req)->flags |= RRF_OPEN;
        REBVAL *result = OS_DO_DEVICE(req, RDC_CONNECT);

        if (result == NULL) {
            //
            // comment said "stays queued", hence seems pending happens
            // the request was taken by the device layer, don't try to free
        }
        else {
            Free_Req(req);  // synchronous completion, we must free

            if (rebDid("error?", result, rebEND))
                rebJumps("FAIL", result, rebEND);

            assert(false); // !!! can this happen?
            rebRelease(result); // ignore result
        }

        RETURN (port); }

    case SYM_CLOSE: {
        REBREQ *req = OS_Make_Devreq(&Dev_Event);

        OS_DO_DEVICE_SYNC(req, RDC_CLOSE);

        Free_Req(req);
        RETURN (port); }

    case SYM_FIND:
        break; // !!! R3-Alpha said "add it" (e.g. unimplemented)

    default:
        break;
    }

    return R_UNHANDLED;
}


//
//  Startup_Event_Scheme: C
//
void Startup_Event_Scheme(void)
{
}


//
//  Shutdown_Event_Scheme: C
//
void Shutdown_Event_Scheme(void)
{
}
