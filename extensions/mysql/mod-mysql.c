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
const char * field_type_to_text(int field_type){
    const char *result; 
    switch ( field_type ) {
        case MYSQL_TYPE_TINY:
            result = "TINYINT";
            break;
        case MYSQL_TYPE_SHORT:
            result = "SMALLINT";
            break;
        case MYSQL_TYPE_LONG:
            result = "INTEGER";
            break;
        case MYSQL_TYPE_INT24:
            result = "MEDIUMINT";
            break;
        case MYSQL_TYPE_LONGLONG:
            result = "BIGINT";
            break;
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
            result = "DECIMAL";
            break;
        case MYSQL_TYPE_FLOAT:
            result = "FLOAT";
            break;
        case MYSQL_TYPE_DOUBLE:
            result = "DOUBLE";
            break;
        case MYSQL_TYPE_BIT:
            result = "BIT";
            break;
        case MYSQL_TYPE_TIMESTAMP:
            result = "TIMESTAMP";
            break;
        case MYSQL_TYPE_DATE:
            result = "DATE";
            break;
        case MYSQL_TYPE_TIME:
            result = "TIME";
            break;
        case MYSQL_TYPE_DATETIME:
            result = "DATETIME";
            break;
        case MYSQL_TYPE_YEAR:
            result = "YEAR";
            break;
        case MYSQL_TYPE_STRING:
            result = "CHAR";
            break;
        case MYSQL_TYPE_VAR_STRING:
            result = "VARCHAR";
            break;
        case MYSQL_TYPE_BLOB:
            result = "BLOB";
            break;
        case MYSQL_TYPE_SET:
            result = "SET";
            break;
        case MYSQL_TYPE_ENUM:
            result = "ENUM";
            break;
        case MYSQL_TYPE_GEOMETRY:
            result = "SPATIAL";
            break;
        case MYSQL_TYPE_NULL:
            result = "NULL";
            break;
        default: 
            result = "UNKNOWN";
            break;
    }
    return result;
}
// End Helper Functions

// "warning: implicit declaration of function"

//
//  export mysql-connect: native [
//
//      {Attempts to establish a connection to a MySQL server running on host}
//
//      return: [handle! void!]
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
        rebJumps("FAIL {Not able to initialize connection using mysql_init}");
        return rebVoid();
    }
    const char * host = cast(char*, VAL_STRING_AT(ARG(host)));
    const char * user = cast(char*, VAL_STRING_AT(ARG(user)));
    const char * pwrd = cast(char*, VAL_STRING_AT(ARG(pwrd)));
    const char * dbnm = cast(char*, VAL_STRING_AT(ARG(dbnm)));
    
    if (mysql_real_connect(connection, host, user, pwrd, 
          dbnm, 0, NULL, 0) == NULL)
    {
        rebJumps("FAIL {Not able to connect using mysql_real_connect}");
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
//  export mysql-get-server-version: native [
//    
//      {Returns the MySQL server version as a number.}
//
//      return: [integer!]
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_get_server_version)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_GET_SERVER_VERSION;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    unsigned int result = mysql_get_server_version(connection);

    return rebInteger(result);
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

//
//  export mysql-get-proto-info: native [
//    
//      {Returns the protocol version of the connection as a number.}
//
//      return: [integer!]
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_get_proto_info)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_GET_PROTO_INFO;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    unsigned int result = mysql_get_proto_info(connection);

    return rebInteger(result);
}

//
//  export mysql-affected-rows: native [
//    
//      {Returns the number of rows changed, deleted, or inserted by the last statement.} 
//
//      return: [integer!]
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_affected_rows)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_AFFECTED_ROWS;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    unsigned int result = mysql_affected_rows(connection);

    return rebInteger(result);
}

//
//  export mysql-field-count: native [
//    
//      {Returns the number of columns for the most recent query on the connection.} 
//
//      return: [integer!]
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_field_count)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_FIELD_COUNT;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    unsigned int result = mysql_field_count(connection);

    return rebInteger(result);
}

//
//  export mysql-num-fields: native [
//    
//      {Returns the number of columns for a resultset.} 
//
//      return: [integer!]
//      resultset [handle!]
//  ]
//
REBNATIVE(mysql_num_fields)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_NUM_FIELDS;

    MYSQL_RES *resultset = VAL_HANDLE_POINTER( MYSQL_RES, ARG(resultset));

    unsigned int result = mysql_num_fields(resultset);

    return rebInteger(result);
}

//
//  export mysql-store-result: native [
//    
//      {Reads the entire result of a query to the client, allocates a structure, and places the result into this structure. } 
//
//      return: [handle!]
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_store_result)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_STORE_RESULT;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    MYSQL_RES *resultset = mysql_store_result(connection);

    return rebHandle(resultset, 0, nullptr);
}

//
//  export mysql-use-result: native [
//    
//      {Initiates a result set retrieval of a query to the client, allocates a structure, does not place the result into this structure like mysql-store-result does. } 
//
//      return: [handle!]
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_use_result)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_USE_RESULT;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    MYSQL_RES *resultset = mysql_use_result(connection);

    return rebHandle(resultset, 0, nullptr);
}

//
//  export mysql-num-rows: native [
//    
//      {Returns the number of rows in the result set.} 
//
//      return: [integer!]
//      resultset [handle!]
//  ]
//
REBNATIVE(mysql_num_rows)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_NUM_ROWS;

    MYSQL_RES *resultset = VAL_HANDLE_POINTER( MYSQL_RES, ARG(resultset));

    unsigned int result = mysql_num_rows(resultset);

    return rebInteger(result);
}

//
//  export mysql-fetch-row: native [
//    
//      {Retrieves the next row of a result set} 
//
//      return: [block!]
//      resultset [handle!]
//  ]
//
REBNATIVE(mysql_fetch_row)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_FETCH_ROW;

    MYSQL_RES *resultset = VAL_HANDLE_POINTER( MYSQL_RES, ARG(resultset));

    int num_fields = mysql_num_fields(resultset);

    MYSQL_ROW row;
    MYSQL_FIELD *field;
    REBVAL *block = rebValue("[]");
    REBVAL *blank = rebBlank();

    if ((row = mysql_fetch_row(resultset)))
    {
        for (int i=0; i < num_fields; i++)
        {
            if (not row[i])
            {
                rebElide("append", block, blank);
            }
            else
            {
                field = mysql_fetch_field_direct( resultset, i);
 
                switch ( field->type ) {
                    case MYSQL_TYPE_STRING:
                    case MYSQL_TYPE_VAR_STRING:
                    case MYSQL_TYPE_BLOB:
                        rebElide("append", block, rebT(row[i]));
                        break;

                    case MYSQL_TYPE_DATE:
                        rebElide("append", block, "make date! replace", rebT(row[i]), "{0000-00-00} {0000-01-01}");
                        break;

                    case MYSQL_TYPE_DATETIME:
                        rebElide("append", block, "make date! replace replace", rebT(row[i]), "{ } {/} {0000-00-00} {0000-01-01}");
                        break;
                  
                    default:
                        rebElide("append", block, row[i]);
                        break;
                }
            }
        }
    }

    rebRelease(blank);

    return block;
}

//
//  export mysql-fetch-field: native [
//    
//      {Retrieves the next field properties of a row in a result set.
//       Returns a block! with the values of:
//         name
//         org_name
//         table
//         org_table
//         type
//         length
//         max_length
//         flags
//         decimals
//         charsetnr
//      } 
//
//      return: [block!]
//      resultset [handle!]
//  ]
//
REBNATIVE(mysql_fetch_field)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_FETCH_FIELD;

    MYSQL_RES *resultset = VAL_HANDLE_POINTER( MYSQL_RES, ARG(resultset));

    MYSQL_FIELD *field;
    REBVAL *block = rebValue("[]");
    const char *field_type;

    field = mysql_fetch_field(resultset);
    
    // Append all properties to the block
    rebElide("append", block, rebT(field->name));
    rebElide("append", block, rebT(field->org_name));
    rebElide("append", block, rebT(field->table));
    rebElide("append", block, rebT(field->org_table));
    field_type = field_type_to_text(field->type);
    rebElide("append", block, rebT(field_type));
    rebElide("append", block, rebI(field->length));
    rebElide("append", block, rebI(field->max_length));
    rebElide("append", block, rebI(field->flags));
    rebElide("append", block, rebI(field->decimals));
    rebElide("append", block, rebI(field->charsetnr));

    return block;
}

//
//  export mysql-fetch-fields: native [
//    
//      {Retrieves a block containing field properties of all field from a row in a result set.
//       Returns a block! of block!s with the values of:
//         name
//         org_name
//         table
//         org_table
//         type
//         length
//         max_length
//         flags
//         decimals
//         charsetnr
//       This function saves recursive calling of mysql-fetch-field.
//      } 
//
//      return: [block!]
//      resultset [handle!]
//  ]
//
REBNATIVE(mysql_fetch_fields)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_FETCH_FIELDS;

    MYSQL_RES *resultset = VAL_HANDLE_POINTER( MYSQL_RES, ARG(resultset));

    MYSQL_FIELD *field;
    REBVAL *block = rebValue("[]");
    REBVAL *collectblock = rebValue("[]");
    const char *field_type;

    while((field = mysql_fetch_field(resultset)))
    {
        // Append all properties to the block
        rebElide("append", block, rebT(field->name));
        rebElide("append", block, rebT(field->org_name));
        rebElide("append", block, rebT(field->table));
        rebElide("append", block, rebT(field->org_table));
        field_type = field_type_to_text(field->type);
        rebElide("append", block, rebT(field_type));
        rebElide("append", block, rebI(field->length));
        rebElide("append", block, rebI(field->max_length));
        rebElide("append", block, rebI(field->flags));
        rebElide("append", block, rebI(field->decimals));
        rebElide("append", block, rebI(field->charsetnr));

        // Append the block to the container and clear the block
        rebElide("append/only", collectblock, "copy", block);
        rebElide("clear", block);
    }

    rebRelease(block);

    return collectblock;
}

//
//  export mysql-free-result: native [
//    
//      {Frees the memory allocated for a result set.} 
//
//      return: [void!]
//      resultset [handle!]
//  ]
//
REBNATIVE(mysql_free_result)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_FREE_RESULT;

    MYSQL_RES *resultset = VAL_HANDLE_POINTER( MYSQL_RES, ARG(resultset));

    mysql_free_result(resultset);

    return rebVoid();
}