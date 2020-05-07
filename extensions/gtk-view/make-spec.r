REBOL []

name: 'GTK-View
source: %gtk-view/mod-gtk-view.c
includes: copy [
    %prep/extensions/gtk-view ;for %tmp-extensions-gtk-view-init.inc
]

libraries: _

options: []