; functions/context/bind.r

[
    e: trap [do to block! ":a"]
    e/id = 'not-bound
]

; bug#50
[blank? context of to word! "zzz"]
; BIND works 'as expected' in object spec
; bug#1549
[
    b1: [self]
    ob: make object! [
        b2: [self]
        set 'a same? first b2 first bind/copy b1 'b2
    ]
    a
]
; bug#1549
; BIND works 'as expected' in function body
[
    b1: [self]
    f: func [/local b2] [
        b2: [self]
        same? first b2 first bind/copy b1 'b2
    ]
    f
]
; bug#1549
; BIND works 'as expected' in REPEAT body
[
    b1: [self]
    repeat i 1 [
        b2: [self]
        same? first b2 first bind/copy b1 'i
    ]
]
; bug#1655
[not head? bind next [1] 'rebol]
; bug#892, bug#216
[y: 'x eval func [<local> x] [x: true get bind y 'x]]

[
    #2086
    bind next block: [a a] use [a] ['a]
    same? 'a first block
]

[
    #1893
    word: eval func [x] ['x] 1
    same? word bind 'x word)]
]
