//
//  File: %mod-mysql.c
//  Summary: "MySQL interface extension"
//  Section: Extension
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012-2020 Rebol Open Source Contributors
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

#include <mysql.h>

// Include the rebol core for the rebnative c-function
#include "sys-core.h"

#include "tmp-mod-mysql.h"

// Helper functions

void finish_with_error(MYSQL *con)
{
  fprintf(stderr, "%s\n", mysql_error(con));
  mysql_close(con);
}

// End Helper Functions

// "warning: implicit declaration of function"

//
//  export mysql-init: native [
//  ]
//
REBNATIVE(mysql_init)
{
  CHAT_INCLUDE_PARAMS_OF_MYSQL_INIT;
}