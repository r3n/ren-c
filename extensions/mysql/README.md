MySQL extension

This extension is hardcoded proof of concept for the MySQL interface.

It uses mysql.h and to compile it needs this and the library file

On Linux this can be installed by:

sudo apt install mysql-client-core-8.0     # version 8.0.20-0ubuntu0.20.04.1, or
sudo apt install mariadb-client-core-10.3  # version 1:10.3.22-1ubuntu1

And the library by:

sudo apt install mysql-client-core-8.0     # version 8.0.20-0ubuntu0.20.04.1, or
sudo apt install mariadb-client-core-10.3  # version 1:10.3.22-1ubuntu1

Compiling needs `mysql_config --cflags --libs`