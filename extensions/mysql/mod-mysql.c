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

#define REBOL_IMPLICIT_END
// Include the rebol core for the rebnative c-function
#include "sys-core.h"

#include "tmp-mod-mysql.h"

// Helper functions

// End Helper Functions

// "warning: implicit declaration of function"

//
//  export mysql-connect: native [
//      "Attempts to establish a connection to a MySQL server running on host"
//      return: [object!]
//      host [text!]
//      user [text!]
//      pwrd [text!]
//      dbnm [text!]
//  ]
//
REBNATIVE(mysql_connect)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_CONNECT;

    MYSQL *connection = mysql_init(NULL);
    if (connection == NULL) 
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        //exit(1);
    }
    REBSTR host = rebSpell(ARG(host));
    REBSTR user = rebSpell(ARG(user));
    REBSTR pwrd = rebSpell(ARG(pwrd));
    REBSTR dbnm = rebSpell(ARG(dbnm));
    
    if (mysql_real_connect(connection, host, user, pwrd, 
          dbnm, 0, NULL, 0) == NULL)
    {
      print("Error connecting");
    } 

    return rebHandle(connection);
}

//
//  export mysql-close: native [
//      "Closes a previously opened connection"
//      return: [logic!]
//      connection [object!]
//  ]
//
REBNATIVE(mysql_close)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_CLOSE;

    REBVAL *connection = ARG(connection);

    mysql_close(connection); 

    return rebLogic(true);
}

//
//  export mysql-get-host-info: native [
//      "Returns a string describing the type of connection in use, including the server host name."
//      return: [text!]
//  ]
//
REBNATIVE(mysql_get_host_info)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_GET_HOST_INFO;

    const char *result = mysql_get_host_info();

    return rebText(result);
}

//
//  export mysql-errno: native [
//      "For the connection specified by mysql, mysql_errno() returns the error code for the most recently invoked API function that can succeed or fail."
//      return: [integer!]
//  ]
//
REBNATIVE(mysql_errno)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_ERRNO;

    unsigned int *result = mysql_errno();

    return rebInteger(result);
}

//
//  export mysql-error: native [
//      "For the connection specified by mysql, mysql_error() returns a null-terminated string containing the error message for the most recently invoked API function that failed. "
//      return: [text!]
//  ]
//
REBNATIVE(mysql_error)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_ERROR;

    const char *result = mysql_error();

    return rebText(result);
}