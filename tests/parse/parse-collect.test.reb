; COLLECT and KEEP keywords in PARSE
;
; Non-keyword COLLECT has issues with binding, but also does not have the
; necessary hook to be able to "backtrack" and remove kept material when a
; match rule containing keeps ultimately fails.  These keywords were initially
; introduced in Red, without backtracking...and affecting the return result.
; In Ren-C, backtracking is implemented, and also it is used to set variables
; (like a SET or COPY) instead of affecting the return result.

(did all [
    parse [1 2 3] [collect x [keep [some integer!]]]
    x = [1 2 3]
])
(did all [
    parse [1 2 3] [collect x [some [keep integer!]]]
    x = [1 2 3]
])
(did all [
    parse [1 2 3] [collect x [keep only [some integer!]]]
    x = [[1 2 3]]
])
(did all [
    parse [1 2 3] [collect x [some [keep only integer!]]]
    x = [[1] [2] [3]]
])

; Collecting non-array series fragments

(did all [
    "bbb" = parse "aaabbb" [collect x [keep [some "a"]]]
    x = ["aaa"]
])
(did all [
    "" = parse "aaabbbccc" [
        collect x [keep [some "a"] some "b" keep [some "c"]]
    ]
    x = ["aaa" "ccc"]
])

; Backtracking (more tests needed!)

(did all [
    [] = parse [1 2 3] [
        collect x [
            keep integer! keep integer! keep text!
            |
            keep integer! keep [some integer!]
        ]
    ]
    x = [1 2 3]
])

; No change to variable on failed match (consistent with Rebol2/R3-Alpha/Red
; behaviors w.r.t SET and COPY)

(did all [
    x: <before>
    null = parse [1 2] [collect x [keep integer! keep text!]]
    x = <before>
])

; Nested collect

(did all [
    did parse [1 2 3 4] [
        collect a [
            keep integer!
            collect b [keep [2 integer!]]
            keep integer!
        ]
        end
    ]

    a = [1 4]
    b = [2 3]
])

; GET-BLOCK! can be used to keep material that did not originate from the
; input series or a match rule.  It does a REDUCE to more closely parallel
; the behavior of a GET-BLOCK! in the ordinary evaluator.
;
(did all [
    [3] = parse [1 2 3] [
        collect x [
            keep integer!
            keep :['a <b> #c]
            keep integer!
        ]
    ]
    x = [1 a <b> #c 2]
])
(did all [
    [3] = parse [1 2 3] [
        collect x [
            keep integer!
            keep only :['a <b> #c]
            keep integer!
        ]
    ]
    x = [1 [a <b> #c] 2]
])
(did all [
    parse [1 2 3] [collect x [keep only :[[a b c]]]]
    x = [[[a b c]]]
])

[
    {KEEP without blocks}
    https://github.com/metaeducation/ren-c/issues/935

    (did all [
        did parse "aaabbb" [collect x [keep some "a" keep some "b"] end]
        x = ["aaa" "bbb"]
    ])

    (did all [
        parse "aaabbb" [collect x [keep to "b"] to end]
        x = ["aaa"]
    ])

    (did all [
        parse "aaabbb" [
            collect outer [
                some [collect inner keep some "a" | keep some "b"]
            ]
        ]
        outer = ["bbb"]
        inner = ["aaa"]
    ])
]

