; datatypes/tuple.r

(tuple? 1.2.3)
(not tuple? 1)
(tuple! = type of 1.2.3)

; Test that scanner compacted forms match forms built from arrays
;
(1.2.3 = to tuple! [1 2 3])
;(1x2 = to tuple! [1 2])  ; !!! TBD when unified with pairs

(error? trap [load "1."])  ; !!! Reserved
(error? trap [load ".1"])  ; !!! Reserved
;(.1 = to tuple! [_ 1])  ; No representation due to reservation
;(1. = to tuple! [1 _])  ; No representation due to reservation

; !!! Should dot be inert?  Is there value to having it as an inert predicate
; form for something like identity that does not execute on its own?  It is
; both leading -and- trailing blank, which suggests non-executability...
; (while slash can argue that trailing slashes execute)
;
('. = to tuple! [_ _])

("1.2.3" = mold 1.2.3)

; minimum
(tuple? make tuple! [])

; there is no longer a maximum (if it won't fit in a cell, it will allocate
; a series)

(tuple? 255.255.255.255.255.255.255)
(
    tuple: load-value "1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1.1"
    did all [
        30 = length of tuple  ; too big to fit in cell on 32-bit -or- 64-bit
        (cfor i 1 30 1 [
            assert [tuple/(i) = 1]
        ] true)
    ]
)


; TO Conversion tests
(
    tests: [
        "a.b.c" [a b c]
        "a b c" [a b c]
        "1.2.3" [1 2 3]
        "1 2 3" [1 2 3]
    ]

    for-each [text structure] tests [
        tuple: ensure tuple! to tuple! text
        assert [(length of tuple) = (length of structure)]
        cfor i 1 (length of tuple) 1 [
            assert [tuple/(i) = structure/(i)]
        ]
    ]
    true
)

; No implicit to binary! from tuple!
(
    a-value: 0.0.0.0
    not equal? to binary! a-value a-value
)

(
    a-value: 0.0.0.0
    equal? equal? to binary! a-value a-value equal? a-value to binary! a-value
)

(equal? 0.0.0 0.0.0)
(not equal? 0.0.1 0.0.0)


; These tests were for padding in R3-Alpha of TUPLE! which is not supported
; by the generalized tuple mechanics.
;
(comment [
    ; tuple! right-pads with 0
    (equal? 1.0.0 1.0.0.0.0.0.0)
    ; tuple! right-pads with 0
    (equal? 1.0.0.0.0.0.0 1.0.0)
] true)
