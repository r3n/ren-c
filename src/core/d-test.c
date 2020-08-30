//
//  File: %d-test.c
//  Summary: "Test routines for things only testable from inside Rebol"
//  Section: debug
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2019 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This file was created in order to have a place to put tests of libRebol.
// A better way to do this would be to include C compilation in the test
// suite against libr3.a, and drive those tests accordingly.  But this would
// involve setting up separate compilation and running those programs with
// CALL.  So this is an expedient way to do it just within a native that is
// built only in certain debug builds.
//

#include "sys-core.h"


//
//  test-librebol: native [
//
//  "libRebol tests (ultimately should build as separate EXEs)"
//
//      return: [text! block!]
//          {Block of test numbers and failures}
//      :value [<end> <opt> any-value!]
//          {Optional argument that may be useful for ad hoc tests}
//  ]
//
REBNATIVE(test_librebol)
{
    INCLUDE_PARAMS_OF_TEST_LIBREBOL;
    UNUSED(ARG(value));

  #if !defined(INCLUDE_TEST_LIBREBOL_NATIVE)
    return Init_Text(  // text! vs. failing to distinguish from test failure
        D_OUT,
        Make_String_UTF8(
            "TEST-LIBREBOL only if #define INCLUDE_TEST_LIBREBOL_NATIVE"
        )
    );
  #else
    REBDSP dsp_orig = DSP;

    // Note: rebEND is not needed when using the API unless using C89
    // compatibility mode (#define REBOL_EXPLICIT_END).  That mode is off by
    // default when you `#include "rebol.h`, but the core interpreter is built
    // with it...so that it still builds with C89 compilers.

    Init_Integer(DS_PUSH(), 1);
    Init_Logic(DS_PUSH(), 3 == rebUnboxInteger("1 +", rebI(2), rebEND));

    Init_Integer(DS_PUSH(), 2);
    intptr_t getter = rebUnboxInteger("api-transient {Hello}", rebEND);
    REBNOD *getter_node = cast(REBNOD*, cast(void*, getter));
    Init_Logic(DS_PUSH(), rebDidQ("{Hello} =", getter_node, rebEND));

    return Init_Block(D_OUT, Pop_Stack_Values(dsp_orig));
  #endif
}
