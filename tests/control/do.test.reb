; functions/control/do.r

; Empty BLOCK! forms will return void if empty
; Empty GROUP! forms will vaporize
[
    (void? (1 + 2, do []))
    (void? (1 + 2, do @[]))
    (void? (1 + 2, do :[]))
    (void? (1 + 2, do [comment "hi"]))
    (void? (1 + 2, do @[comment "hi"]))
    (void? (1 + 2, do :[comment "hi"]))

    (3 = (1 + 2, do '()))
    (3 = (1 + 2, do @()))
    (3 = (1 + 2, do ':()))
    (3 = (1 + 2, do '(comment "hi")))
    (3 = (1 + 2, do @(comment "hi")))
    (3 = (1 + 2, do ':(comment "hi")))
]


(
    success: false
    do [success: true]
    success
)
(1 == reeval :abs -1)
(
    a-value: to binary! "1 + 1"
    2 == do a-value
)
(
    a-value: charset ""
    same? a-value reeval a-value
)
; do block start
(:abs = do [:abs])
(
    a-value: #{}
    same? a-value do reduce [a-value]
)
(
    a-value: charset ""
    same? a-value do reduce [a-value]
)
(
    a-value: []
    same? a-value do reduce [a-value]
)
(same? blank! do reduce [blank!])
(1/Jan/0000 = do [1/Jan/0000])
(0.0 == do [0.0])
(1.0 == do [1.0])
(
    a-value: me@here.com
    same? a-value do reduce [a-value]
)
(error? do [trap [1 / 0]])
(
    a-value: %""
    same? a-value do reduce [a-value]
)
(
    a-value: does []
    same? :a-value do [:a-value]
)
(
    a-value: first [:a-value]
    :a-value == do reduce [:a-value]
)
(NUL == do [NUL])
(
    a-value: make image! 0x0
    same? a-value do reduce [a-value]
)
(0 == do [0])
(1 == do [1])
(#a == do [#a])
(
    a-value: first ['a/b]
    :a-value == do [:a-value]
)
(
    a-value: first ['a]
    :a-value == do [:a-value]
)
(#[true] == do [#[true]])
(#[false] == do [#[false]])
($1 == do [$1])
(same? :append do [:append])
(blank? do [_])
(
    a-value: make object! []
    same? :a-value do reduce [:a-value]
)
(
    a-value: first [()]
    same? :a-value do [:a-value]
)
(same? get '+ do [get '+])
(0x0 == do [0x0])
(
    a-value: 'a/b
    :a-value == do [:a-value]
)
(
    a-value: make port! http://
    port? do reduce [:a-value]
)
(/a == do [/a])
(
    a-value: first [a/b:]
    :a-value == do [:a-value]
)
(
    a-value: first [a:]
    :a-value == do [:a-value]
)
(
    a-value: ""
    same? :a-value do reduce [:a-value]
)
(
    a-value: make tag! ""
    same? :a-value do reduce [:a-value]
)
(0:00 == do [0:00])
(0.0.0 == do [0.0.0])
(void? do [()])
('a == do ['a])
; do block end
(
    a-value: blank!
    same? a-value reeval a-value
)
(1/Jan/0000 == reeval 1/Jan/0000)
(0.0 == reeval 0.0)
(1.0 == reeval 1.0)
(
    a-value: me@here.com
    same? a-value reeval a-value
)
(error? trap [do trap [1 / 0] 1])
(
    a-value: does [5]
    5 == reeval :a-value
)
(
    a: 12
    a-value: first [:a]
    :a == reeval :a-value
)
(NUL == reeval NUL)
(
    a-value: make image! 0x0
    same? a-value reeval a-value
)
(0 == reeval 0)
(1 == reeval 1)
(#a == reeval #a)

[#2101 #1434 (
    a-value: first ['a/b]
    all [
        lit-path? a-value
        path? reeval :a-value
        (as path! :a-value) == (reeval :a-value)
    ]
)]

(
    a-value: first ['a]
    all [
        lit-word? a-value
        word? reeval :a-value
        (to-word :a-value) == (reeval :a-value)
    ]
)
(true = reeval true)
(false = reeval false)
($1 == reeval $1)
(null? reeval (specialize :of [property: 'type]) null)
(null? do _)
(
    a-value: make object! []
    same? :a-value reeval :a-value
)
(
    a-value: first [(2)]
    2 == do as block! :a-value
)
(
    a-value: 'a/b
    a: make object! [b: 1]
    1 == reeval :a-value
)
(
    a-value: make port! http://
    port? reeval :a-value
)
(
    a-value: first [a/b:]
    all [
        set-path? :a-value
        error? trap [reeval :a-value]  ; no value to assign after it...
    ]
)
(
    a-value: "1"
    1 == do :a-value
)
(void? do "")
(1 = do "1")
(3 = do "1 2 3")
(
    a-value: make tag! ""
    same? :a-value reeval :a-value
)
(0:00 == reeval 0:00)
(0.0.0 == reeval 0.0.0)
(
    a-value: 'b-value
    b-value: 1
    1 == reeval :a-value
)
; RETURN stops the evaluation
(
    f1: func [] [do [return 1 2] 2]
    1 = f1
)
; THROW stops evaluation
(
    1 = catch [
        do [
            throw 1
            2
        ]
        2
    ]
)
; BREAK stops evaluation
(
    null? loop 1 [
        do [
            break
            2
        ]
        2
    ]
)
; evaluate block tests
(
    success: false
    evaluate [success: true success: false]
    success
)
(
    [b value]: evaluate [1 2]
    did all [
        1 = value
        [2] = b
    ]
)
(
    value: <overwritten>
    did all [
        null? [_ value]: evaluate []
        undefined? 'value
    ]
)
(
    [_ value]: evaluate [trap [1 / 0]]
    error? value
)
(
    f1: func [] [evaluate [return 1 2] 2]
    1 = f1
)
; recursive behaviour
(1 = do [do [1]])
(1 = do "do [1]")
(1 == 1)
(3 = reeval :reeval :add 1 2)
; infinite recursion for block
(
    blk: [do blk]
    error? trap blk
)
; infinite recursion for string
[#1896 (
    str: "do str"
    error? trap [do str]
)]
; infinite recursion for evaluate
(
    blk: [b: evaluate blk]
    error? trap blk
)

; evaluating quoted argument
(
    rtest: func ['op [word!] 'thing] [reeval op thing]
    -1 = rtest negate 1
)
