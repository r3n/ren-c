//
//  File: %mod-mysql.c
//  Summary: "MySQL interface extension"
//  Section: Extension
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012-2021 Ren-C Open Source Contributors
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
// See README.md for notes about this extension.
//

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
//  export mysql-ping: native [
//    
//      "Checks whether the connection to the server is working."
//
//      return: [integer!] "Zero if the connection to the server is active. Nonzero if an error occurred."
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_ping)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_PING;

    MYSQL *connection = VAL_HANDLE_POINTER(MYSQL, ARG(connection));

    unsigned int result = mysql_ping(connection);

    return rebInteger(result);
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
//      "For the connection specified mysql-errno returns the error code for the most recently invoked API function that can succeed or fail."
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
//      "For the connection specified mysql-error returns a null-terminated string containing the error message for the most recently invoked API function that failed."
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
//  export mysql-warning-count: native [
//    
//      "For the connection specified mysql-warning-count returns the error code for the most recently invoked API function that can succeed or fail."
//
//      return: [integer!] "Number of errors, warnings, and notes generated during execution of the previous SQL statement."
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_warning_count)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_WARNING_COUNT;

    MYSQL *connection = VAL_HANDLE_POINTER(MYSQL, ARG(connection));

    unsigned int result = mysql_warning_count(connection);

    return rebInteger(result);
}

//
//  export mysql-character-set-name: native [
//    
//      {Returns a string describing the type of connection in use, including the server host name.} 
//
//      return: [text!]
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_character_set_name)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_CHARACTER_SET_NAME;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    const char *result = mysql_character_set_name(connection);

    return rebText(result);
}

//
//  export mysql-get-character-set-info: native [
//    
//      {Provides a block with information about the default client character set. The default character set may be changed with the mysql-set-character-set.} 
//
//      return: [block!] {character set information:
//  - character set+collation number
//  - characterset name
//  - collation name
//  - comment
//  - directory (can be null, in which case blank! returned)
//  - multi byte character min. length
//  - multi byte character max. length
//  }
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_get_character_set_info)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_GET_CHARACTER_SET_INFO;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));
    MY_CHARSET_INFO cs;

    mysql_get_character_set_info(connection, &cs);
   
    // Append all info to the output
    REBVAL *block = rebValue("[]");
    REBVAL *blank = rebBlank();

    rebElide("append", block, rebI(cs.number));
    rebElide("append", block, rebT(cs.name));
    rebElide("append", block, rebT(cs.csname));
    rebElide("append", block, rebT(cs.comment));
    if (cs.dir == NULL){
        rebElide("append", block, blank);
    }
    else {
        rebElide("append", block, rebT(cs.dir));
    }
    rebElide("append", block, rebI(cs.mbminlen));
    rebElide("append", block, rebI(cs.mbmaxlen));

    rebRelease(blank);

    return block;
}

//
//  export mysql-set-character-set: native [
//    
//      { This function is used to set the default character set for the current connection. 
//        The string csname specifies a valid character set name. The connection collation becomes the default collation of the character set. 
//      } 
//
//      return: [integer!] {Zero for success. Nonzero if an error occurred.}
//      connection [handle!]
//      csname [text!]
//  ]
//
REBNATIVE(mysql_set_character_set)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_SET_CHARACTER_SET;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));
    const char * csname = cast(char*, VAL_STRING_AT(ARG(csname)));

    int result = mysql_set_character_set(connection, csname);

    return rebInteger(result);
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
//  export mysql-fetch-field-direct: native [
//    
//      {Retrieves the field properties of the requested field from a row in a result set.
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
//      fieldnumber [integer!]
//  ]
//
REBNATIVE(mysql_fetch_field_direct)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_FETCH_FIELD_DIRECT;

    MYSQL_RES *resultset = VAL_HANDLE_POINTER( MYSQL_RES, ARG(resultset));
    unsigned int fieldnumber = rebUnboxInteger(ARG(fieldnumber));

    MYSQL_FIELD *field;
    REBVAL *block = rebValue("[]");
    const char *field_type;

    field = mysql_fetch_field_direct(resultset, fieldnumber);
    
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

    unsigned int num_fields;
    unsigned int i;
    MYSQL_FIELD *fields;
    REBVAL *block = rebValue("[]");
    REBVAL *collectblock = rebValue("[]");
    const char *field_type;

    num_fields = mysql_num_fields(resultset);
    fields = mysql_fetch_fields(resultset);

    for(i = 0; i < num_fields; i++){
        // Append all properties to the block
        rebElide("append", block, rebT(fields[i].name));
        rebElide("append", block, rebT(fields[i].org_name));
        rebElide("append", block, rebT(fields[i].table));
        rebElide("append", block, rebT(fields[i].org_table));
        field_type = field_type_to_text(fields[i].type);
        rebElide("append", block, rebT(field_type));
        rebElide("append", block, rebI(fields[i].length));
        rebElide("append", block, rebI(fields[i].max_length));
        rebElide("append", block, rebI(fields[i].flags));
        rebElide("append", block, rebI(fields[i].decimals));
        rebElide("append", block, rebI(fields[i].charsetnr));

        // Append the block to the container and clear the block
        rebElide("append/only", collectblock, "copy", block);
        rebElide("clear", block);
    }

    rebRelease(block);

    return collectblock;
}

//
//  export mysql-fetch-lengths: native [
//    
//      {Retrieves a block containing field lengths of current row in a result set.} 
//
//      return: [block!]
//      resultset [handle!]
//  ]
//
REBNATIVE(mysql_fetch_lengths)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_FETCH_LENGTHS;

    MYSQL_RES *resultset = VAL_HANDLE_POINTER( MYSQL_RES, ARG(resultset));

    unsigned int num_fields;
    unsigned long *lengths;
    unsigned int i;
    REBVAL *block = rebValue("[]");

    num_fields = mysql_num_fields(resultset);
    lengths = mysql_fetch_lengths(resultset);

    for(i = 0; i < num_fields; i++){
        rebElide("append", block, rebI(lengths[i]));
    }

    return block;
}

//
//  export mysql-insert-id: native [
//    
//      {Returns the value generated for an AUTO_INCREMENT column by the previous INSERT or UPDATE statement.} 
//
//      return: [integer!]
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_insert_id)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_INSERT_ID;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    unsigned int result = mysql_insert_id(connection);

    return rebInteger(result);
}

//
//  export mysql-data-seek: native [
//    
//      {  Seeks to an arbitrary row in a query result set. The offset value is a row number. 
//         Specify a value in the range from 0 to mysql-num-rows - 1.
//         This function requires that the result set structure contains the entire result of the query, 
//         so mysql-data-seek may be used only in conjunction with mysql-store-result, not with mysql-use-result. 
//      } 
//
//      return: [void!]
//      resultset [handle!]
//      offset [integer!]
//  ]
//
REBNATIVE(mysql_data_seek)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_DATA_SEEK;

    MYSQL_RES *resultset = VAL_HANDLE_POINTER( MYSQL_RES, ARG(resultset));
    unsigned int offset = rebUnboxInteger(ARG(offset));

    mysql_data_seek(resultset, offset);

    return rebVoid();
}

//
//  export mysql-field-seek: native [
//    
//      { Sets the field cursor to the given offset. The next call to mysql-fetch-field retrieves the field definition of the column associated with that offset.
//        To seek to the beginning of a row, pass an offset value of zero.
//      } 
//
//      return: [integer!] {The previous value of the field cursor.}
//      resultset [handle!]
//      offset [integer!]
//  ]
//
REBNATIVE(mysql_field_seek)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_FIELD_SEEK;

    MYSQL_RES *resultset = VAL_HANDLE_POINTER( MYSQL_RES, ARG(resultset));
    unsigned int offset = rebUnboxInteger(ARG(offset));

    unsigned int result = mysql_field_seek(resultset, offset);

    return rebInteger(result);
}

//
//  export mysql-field-tell: native [
//    
//      {Returns the position of the field cursor used for the last mysql-fetch-field.} 
//
//      return: [integer!]
//      resultset [handle!]
//  ]
//
REBNATIVE(mysql_field_tell)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_FIELD_TELL;

    MYSQL_RES *resultset = VAL_HANDLE_POINTER( MYSQL_RES, ARG(resultset));

    unsigned int result = mysql_field_tell(resultset);

    return rebInteger(result);
}

//
//  export mysql-row-seek: native [
//    
//      {  Sets the row cursor to an arbitrary row in a query result set. 
//         The offset value is a row offset, typically a value returned from mysql-row-tell or from mysql-row-seek. 
//         This value is not a row number; to seek to a row within a result set by number, use mysql-data-seek instead.
//         This function requires that the result set structure contains the entire result of the query, 
//         so mysql-row-seek may be used only in conjunction with mysql-store-result, not with mysql-use-result.
//      } 
//
//      return: [handle!] {The previous value of the row cursor. This value may be passed to a subsequent call to mysql-row-seek.}
//      resultset [handle!]
//      offset [handle!]
//  ]
//
REBNATIVE(mysql_row_seek)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_ROW_SEEK;

    MYSQL_RES *resultset = VAL_HANDLE_POINTER( MYSQL_RES, ARG(resultset));
    MYSQL_ROW_OFFSET offset = VAL_HANDLE_VOID_POINTER(ARG(offset));

    MYSQL_ROW_OFFSET result = mysql_row_seek(resultset, offset);

    return rebHandle(result, 0, nullptr);
}

//
//  export mysql-row-tell: native [
//    
//      { Returns the current position of the row cursor for the last mysql-fetch-row. This value can be used as an argument to mysql-row-seek.
//        Use mysql-row-tell only after mysql-store-result, not after mysql-use-result. } 
//
//      return: [handle!]
//      resultset [handle!]
//  ]
//
REBNATIVE(mysql_row_tell)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_ROW_TELL;

    MYSQL_RES *resultset = VAL_HANDLE_POINTER( MYSQL_RES, ARG(resultset));

    MYSQL_ROW_OFFSET result = mysql_row_tell(resultset);

    return rebHandle(result, 0, nullptr);
}

//
//  export mysql-sqlstate: native [
//    
//      {Returns a null-terminated string containing the SQLSTATE error code for the most recently executed SQL statement.
//       The error code consists of five characters. '00000' means “no error.” The values are specified by ANSI SQL and ODBC.}
//
//      return: [text!] {A null-terminated character string containing the SQLSTATE error code.}
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_sqlstate)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_SQLSTATE;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    const char *result = mysql_sqlstate(connection);

    return rebText(result);
}

//
//  export mysql-stat: native [
//    
//      {Returns a character string containing information similar to that provided by the mysqladmin status command. This includes uptime in seconds and the number of running
//       threads, questions, reloads, and open tables. }
//
//      return: [text! void!] {A character string describing the server status. NULL if an error occurred.}
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_stat)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_STAT;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    const char *result = mysql_stat(connection);

    if (result == NULL){ 
        return rebVoid(); 
    }

    return rebText(result);
}

//
//  export mysql-more-results: native [
//    
//      {Used when you execute multiple statements specified as a single statement string.}
//
//      return: [logic!] "TRUE (1) if more results exist. FALSE (0) if no more results exist."
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_more_results)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_MORE_RESULTS;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    bool result = mysql_more_results(connection);
    REBVAL *logic = rebValue("TO LOGIC!", result);

    return rebValue(logic);
}

//
//  export mysql-next-result: native [
//    
//      {Reads the next statement result and returns a status to indicate whether more results exist.}
//
//      return: [integer!] {0  Successful and there are more results
//  -1  Successful and there are no more results
//  >0  An error occurred}
//      connection [handle!]
//  ]
//
REBNATIVE(mysql_next_result)
{
    MYSQL_INCLUDE_PARAMS_OF_MYSQL_NEXT_RESULT;

    MYSQL *connection = VAL_HANDLE_POINTER( MYSQL, ARG(connection));

    int result = mysql_next_result(connection);

    return rebInteger(result);
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