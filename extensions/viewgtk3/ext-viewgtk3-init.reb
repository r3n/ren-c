REBOL [
    Title: "viewGTK3 Extension"
    Name: viewGTK3
    Type: Module
    Options: [isolate]
    Version: 1.0.0
    License: {LGPL 3.0}
]

; Temporary DOB object, until time to figure out how to use and extend the GOB! type.
dob: make object! [
    id: ""
    name: ""
    handle: 0
    offset: 0x0
    size: 100x100
    type: 'notype
    parent: make object! []
    children: copy []
    visible: false
]


; Functions for viewGTK3 extension coded with Ren-C.
view: function [][
    print "Not implemented yet!"
]

layout: function [][
    print "Not implemented yet!"
]

show: function [ {Show the widget and its children}
    h [handle!] "The handle of the widget"
    <local> child
][

]

; Sum up all functions that should be known in Ren-C.
export [dob layout show view]