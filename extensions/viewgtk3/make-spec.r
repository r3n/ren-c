REBOL []

name: 'viewGTK3
source: %viewgtk3/mod-viewgtk3.c
includes: copy [
    %prep/extensions/viewgtk3 ;for %tmp-extensions-viewgtk3-init.inc
]

libraries: _

options: []