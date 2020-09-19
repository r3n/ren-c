## MySQL extension

This extension provides functions to connect to and use a MySQL database.
Using the MySQL database makes it possible to use RENC as replacement programming language for PHP.

To compile the extension it needs the *mysql.h* file and the development library to be present on the system.

On Linux the mysql.h file can be installed by:

    sudo apt install mysql-client-core-8.0     # version 8.0.20-0ubuntu0.20.04.1, or
    sudo apt install mariadb-client-core-10.3  # version 1:10.3.22-1ubuntu1

And the development library by:

    sudo apt install libmysqlclient-dev

When compiling the extension the flag \`mysql_config --cflags --libs` will have to be present, like added in the rebmake.r file.

## Implemented functions

Basic functions

    mysql-affected-rows
    mysql-character-set-name
    mysql-close
    mysql-connect (uses mysql_init and mysql_real_connect)
    mysql-data-seek
    mysql-errno
    mysql-error
    mysql-fetch-field
    mysql-fetch-field-direct
    mysql-fetch-fields
    mysql-fetch-lengths
    mysql-fetch-row (uses also mysql_fetch_field_direct)
    mysql-field-count
    mysql-field-seek
    mysql-field-tell
    mysql-free-result
    mysql-get-character-set-info
    mysql-get-client-info
    mysql-get-host-info
    mysql-get-proto-info
    mysql-get-server-info
    mysql-get-server-version
    mysql-insert-id
    mysql-num-fields
    mysql-num-rows
    mysql-query
    mysql-row-seek
    mysql-row-tell
    mysql-set-character-set
    mysql-sqlstate
    mysql-stat
    mysql-store-result
    mysql-use-result

Thread functions

    N.A.


## Typical use of RENC in combination with MySQL

The use of the MySQL interface is straightforward and best illustrated with some example code.

### Example code 

Make sure the files have the proper right to execute on the server (chmod 755) and use the proper path to the **renc** executable in the cgi-bin directory.

You can choose to use the *.cgi* extension for your script or add your own extension like *.renc*

If you choose to add the *.renc* extension then add lines to a (new) *.htaccess* file in the directory

    Options +ExecCGI
    AddHandler cgi-script cgi pl reb renc r
In this example the .renc file extension is added besides the common cgi and Perl (pl) and already present .r and .reb file extensions, that were in use by earlier **Rebol** versions. The *.htaccess* is valid in all subdirectories of the directory it is placed inside.

File *mysql-db.renc*

    REBOL []
    dbname:     "your database name"
    dbhostname: "localhost"
    dbusername: "username"
    dbpassword: "yoursecretpassword"

File *mysql-test.renc*

    #!/yourpathtothe/cgi-bin/renc

    REBOL [Title: "MySQL usage example"]

    print "Content-type: text/html^/"

    do %mysql-db.renc
    dbhandle: mysql-connect dbhostname dbusername dbpassword dbname
    print "DB handle opened!"

    query-text: {SELECT * FROM `test`} 
    mysql-query dbhandle query-text
    print "<br />"
    print "Query executed!" 

    result: mysql-store-result dbhandle
    print "<br />"
    print "Query result stored"

    until [
        row: mysql-fetch-row result
        print "<br />"
        probe row
        empty? row
    ]

    mysql-free-result result
    print "<br />"
    print "Result freed!"

    print "<br />"
    mysql-close dbhandle
    print "<br />"
    print "DB handle closed!"
    print "</BODY></HTML>"
Note that the result of the compilation is the executable named **r3**, here it is renamed to **renc**.

## MySQL field types

MySQL field types are numbered by enum mechanism. This is used to adapt the return values to suit the RENC(Rebol) types.

Date/Datetime: a date value of 0000-00-00 will not be recognized by RENC.
The value will be converted to the first valid date value, 0000-01-01.

Logic type: MySQL knows BOOLEAN, but in practise uses a TINYINT for representation.
Value 0 is false, other values will be true. To not lose the extra possible information when other values
than 0 and 1 are used, the value of the TINYINT will be passed as is and leave it to the RENC program to handle its value, like turn it in a true logic! value.

String types: Will be passed as text! value

Binary/BLOB type: Not tested yet, if needed appropriate action must be implemented.

## MySQL on your hosting solution

If you want to use the MySQL with your hosting provider, chances are you have to compile
the RENC program with the MySQL extension on a similar machine as the provider is 
offering.

After installing the correct OS on a VM the MySQL files must be installed.

Example CentOS (7)
    su -                           // switch to root user
    yum install mysql mysql-devel

When compiling on CentOS (7) you will need to add 'standard: gnu99' to your compile line. (*1)

    "$R3_MAKE" ../make.r config: ../configs/mysql-config.r debug: asserts optimize: 2 standard: gnu99

Note there is a mysql-config.r file because the MySQL containing executable does not need every other component, for example view.
And from reverse angle, not all RENC executables will need MySQL binding, and the dependencies to compile coming from it.
The executable created is named **r3**, you are free to rename this into anything you like, like **renc** for example.

### Tip for developing and compiling on a virtual machine

When developing and or compiling on a virtual machine, use a separate terminal window or tab for different tasks, this way the task to be performed is always under the arrow up key. 
Use one for the compiling step of the extension itself, one for the linking step, one for the complete build, one for the copying of the source and produced executable to the host machine... 

## Notes

(*1)  The change from my_bool to bool in MySQL8.0 means that the mysql.h header file requires a C++ or C99 compiler to compile. (source: https://dev.mysql.com/doc/c-api/8.0/en/c-api-data-structures.html )