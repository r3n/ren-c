; datatypes/gob.r
; minimum
(gob? make gob! [])
(gob! = type of make gob! [])

[#202 (
    1 = index of make gob! []
)]

[#62 (
    g: make gob! []
    1x1 == g/offset: 1x1
)]
[#1969 (
    g1: make gob! []
    g2: make gob! []
    insert g1 g2
    same? g1 g2/parent
    do "g1: _"
    do "recycle"
    g3: make gob! []
    insert g2/parent g3
    true
)]
(
    main: make gob! []
    for-each i [31 325 1] [
        clear main
        recycle
        loop i [
            append main make gob! []
        ]
    ]
    true
)

[#301 (
    'expect-val = pick trap [make gob! [path/size: 10x10]] 'id
)]

[#203 (
    g: make gob! 10x20
    g/offset = 10x20
)]

(
    gob: make gob! 10x20
    did all [
        0 = length of gob
        append gob make gob! 3x4
        1 = length of gob
        gob/1/offset = 3x4
    ]
)

(
    g1: make gob! []
    g2: make gob! []
    append g1 g2
    did all [
        g2.parent = g1
        g1.parent = null
    ]
)

[#1797 (
    a: make gob! []
    repend a [
        make gob! [text: "1"] make gob! [text: "2"] make gob! [text: "3"]
    ]
    b: take/part next a 1
    did all [
        1 = length of b
        b/1/text = "2"
        2 = length of a
        (first a)/text = "1"
        a/2/text = "3"
    ]
)]
