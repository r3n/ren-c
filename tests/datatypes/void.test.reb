; datatypes/unset.r

(not void? 1)

; Unlabeled VOID! (console hides as eval result, so no `== ~`, just vanishes)
;
(void? ~)
(null = label of ~)

; Labeled VOID!s can be literal, or created from WORD!
(
    v: make void! 'labeled
    did all [
        void? get/any 'v
        undefined? 'v
        ~labeled~ = get/any 'v
        'labeled = label of get/any 'v
    ]
)

; ~void~ is the de-facto stanard for the return value of functions when
; they are not supposed to return a usable value
(
    data: [a b c]
    f: func [return: <void>] [append data [1 2 3]]
    ~void~ = f
)

; ~empty~ is the response for when there is no content; this applies to
; functions as well if they do not explicitly force to void.
;
(~empty~ = do [])
(
    foo: func [] []
    ~empty~ = foo
)
(~empty~ = applique 'foo [])
(~empty~ = do :foo)

; ~void~ is the more formal convention for what you get by RETURN with no
; argument, or if the spec says <void> any result.
(
    foo: func [return: <void>] []
    ~void~ = foo
)(
    foo: func [] [return]
    ~void~ = foo
)
(~void~ = applique 'foo [])
(~void~ = do :foo)

; ~local~ is the type of locals before they are assigned
(
    f: func [<local> loc] [get/any 'loc]
    f = ~local~
)

; ~undefined~ is the type of things that just were never declared
(
    ~undefined~ = get/any 'asiieiajiaosdfbjakbsjxbjkchasdf
)

; MATCH will match a void as-is, but falsey inputs produce ~matched~
[
    (~preserved~ = match void! ~preserved~)
    (~matched~ = match null null)
]

; CYCLE differentiates a STOP result from BREAK with STOPPED
[
    (~stopped~ = cycle [stop])
    (~custom~ = cycle [stop ~custom~])
    (null = cycle [break])
]

; ~branched~ is used both as a way to give conditionals whose branches yield
; NULL a non-null result, as well as a "relabeling" of voids that are in
; the branch.  To avoid the relabeling, use the @ forms of branch.
[
    (~branched~ = if true [null])
    (~branched~ = if true [])
    (~branched~ = if true [~overwritten~])

    (null = if true @[null])
    (~empty~ = if true @[])
    (~untouched~ = if true @[~untouched~])
]

; ~quit~ is the label of the VOID! you get by default from QUIT
; Note: DO of BLOCK! does not catch quits, so TEXT! is used here.
[
    (1 = do "quit 1")
    (~quit~ = do "quit")
    (~unmodified~ = do "quit ~unmodified~")
]

; Erroring modes of VOID! are being fetched by WORD! and logic tests.
; They are inert values otherwise, so PARSE should treat them such.
;
(did parse [~foo~ ~foo~] [some ~foo~])  ; acceptable
(
    foo: ~foo~
    e: trap [
        parse [~foo~ ~foo~] [some foo]  ; not acceptable
    ]
    e/id = 'need-non-void
)

(
    is-barrier?: func [x [<end> integer!]] [null? x]
    is-barrier? ()
)

[#68 https://github.com/metaeducation/ren-c/issues/876
    ('need-non-end = (trap [a: ()])/id)
]


(error? trap [a: ~void~ a])
(not error? trap [set 'a ~void~])

(
    a-value: ~void~
    e: trap [a-value]
    e/id = 'need-non-void
)
