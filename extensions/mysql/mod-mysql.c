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
//
//      {Attempts to establish a connection to a MySQL server running on host}
//
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
        return rebVoid();
        // fprintf(stderr, "%s\n", mysql_error(con));
        //exit(1);
    }
    const char * host = cast(char*, VAL_STRING_AT(ARG(host)));
    const char * user = cast(char*, VAL_STRING_AT(ARG(user)));
    const char * pwrd = cast(char*, VAL_STRING_AT(ARG(pwrd)));
    const char * dbnm = cast(char*, VAL_STRING_AT(ARG(dbnm)));
    
    if (mysql_real_connect(connection, host, user, pwrd, 
          dbnm, 0, NULL, 0) == NULL)
    {
        return rebVoid();
    } 

    return rebHandle(connection, 0, nullptr);
}

//
//  export mysql-close: native [
//    
//      {Closes a previously opened connection}
//
//      return: [logic!]
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_close)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_CLOSE;

//zmq_msg_t *msg = VAL_HANDLE_POINTER(zmq_msg_t, ARG(msg));
    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    mysql_close(connection); 

    return rebLogic(true);
}

//
//  export mysql-query: native [
//    
//      "Executes the SQL statement"
//
//      return: [integer!]
//      connection [handle!]
//      statement [text!]
//  ]
//
REBNATIVE(mysql_query)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_QUERY;

    MYSQL *connection = VAL_HANDLE_POINTER(MYSQL, ARG(connection));
    const char * statement = cast(char*, VAL_STRING_AT(ARG(statement)));

    unsigned int result = mysql_query(connection, statement);

    return rebInteger(result);
}

//
//  export mysql-errno: native [
//    
//      "For the connection specified mysql_errno() returns the error code for the most recently invoked API function that can succeed or fail."
//
//      return: [integer!]
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_errno)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_ERRNO;

    MYSQL *connection = VAL_HANDLE_POINTER(MYSQL, ARG(connection));

    unsigned int result = mysql_errno(connection);

    return rebInteger(result);
}

//
//  export mysql-error: native [
//
//      "For the connection specified mysql_error() returns a null-terminated string containing the error message for the most recently invoked API function that failed. "
//
//      return: [text!]
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_error)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_ERROR;

    MYSQL *connection = VAL_HANDLE_POINTER(MYSQL, ARG(connection));

    const char *result = mysql_error(connection);

    return rebText(result);
}

//
//  export mysql-get-client-info: native [
//    
//      {Returns a string that represents the MySQL client library version (for example, "5.7.32")}
//
//      return: [text!]
//  ]
//
REBNATIVE(mysql_get_client_info)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_GET_CLIENT_INFO;

    const char *result = mysql_get_client_info();

    return rebText(result);
}

//
//  export mysql-get-server-info: native [
//    
//      {Returns a string that represents the MySQL server version (for example, "5.7.32").}
//
//      return: [text!]
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_get_server_info)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_GET_SERVER_INFO;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    const char *result = mysql_get_server_info(connection);

    return rebText(result);
}

//
//  export mysql-get-host-info: native [
//    
//      {Returns a string describing the type of connection in use, including the server host name.} 
//
//      return: [text!]
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_get_host_info)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_GET_HOST_INFO;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    const char *result = mysql_get_host_info(connection);

    return rebText(result);
}
