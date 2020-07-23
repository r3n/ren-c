REBOL []

name: 'MySQL
source: %mysql/mod-mysql.c
includes: copy [
    %prep/extensions/mysql ;for %tmp-extensions-mysql-init.inc
]

libraries: _

options: []