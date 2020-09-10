; functions/secure/protect.r
; block
[#1748 (
    value: copy original: [1 + 2 + 3]
    protect value
    all [
        error? trap [insert value 4]
        equal? value original
    ]
)]
(
    value: copy original: [1 + 2 + 3]
    protect value
    all [
        error? trap [append value 4]
        equal? value original
    ]
)
(
    value: copy original: [1 + 2 + 3]
    protect value
    all [
        error? trap [change value 4]
        equal? value original
    ]
)
(
    value: copy original: [1 + 2 + 3]
    protect value
    all [
        error? trap [poke value 1 4]
        equal? value original
    ]
)
(
    value: copy original: [1 + 2 + 3]
    protect value
    all [
        error? trap [remove/part value 1]
        equal? value original
    ]
)
(
    value: copy original: [1 + 2 + 3]
    protect value
    all [
        error? trap [take value]
        equal? value original
    ]
)
(
    value: copy original: [1 + 2 + 3]
    protect value
    all [
        error? trap [reverse value]
        equal? value original
    ]
)
(
    value: copy original: [1 + 2 + 3]
    protect value
    all [
        error? trap [clear value]
        equal? value original
    ]
)
; string
(
    value: copy original: {1 + 2 + 3}
    protect value
    all [
        error? trap [insert value 4]
        equal? value original
    ]
)
(
    value: copy original: {1 + 2 + 3}
    protect value
    all [
        error? trap [append value 4]
        equal? value original
    ]
)
(
    value: copy original: {1 + 2 + 3}
    protect value
    all [
        error? trap [change value 4]
        equal? value original
    ]
)
(
    value: copy original: {1 + 2 + 3}
    protect value
    all [
        error? trap [poke value 1 4]
        equal? value original
    ]
)
(
    value: copy original: {1 + 2 + 3}
    protect value
    all [
        error? trap [remove/part value 1]
        equal? value original
    ]
)
(
    value: copy original: {1 + 2 + 3}
    protect value
    all [
        error? trap [take value]
        equal? value original
    ]
)
(
    value: copy original: {1 + 2 + 3}
    protect value
    all [
        error? trap [reverse value]
        equal? value original
    ]
)
(
    value: copy original: {1 + 2 + 3}
    protect value
    all [
        error? trap [clear value]
        equal? value original
    ]
)
[#1764
    (unset 'blk protect/deep 'blk true)
]
(unprotect 'blk true)


; TESTS FOR TEMPORARY EVALUATION HOLDS
; These should elaborated on, and possibly be in their own file.  Simple tests
; for now.

('series-held = pick trap [do code: [clear code]] 'id)
(
    obj: make object! [x: 10]
    'series-held = pick trap [do code: [obj/x: (clear code recycle 20)]] 'id
)
