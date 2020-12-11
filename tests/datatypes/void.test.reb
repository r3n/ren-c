; datatypes/unset.r

(not void? 1)

; Labeled VOID!s can be literal, or created from WORD!
(
    v: make void! 'labeled
    did all [
        void? get/any 'v
        undefined? 'v
        '~labeled~ = get/any 'v
        'labeled = label of get/any 'v
    ]
)

; Plain ~ is not a void, but a WORD!.  But things like ~~~ are not WORD!,
; because that would be ambiguous with a VOID! with the word-label of ~.
; So ~ is the only "~-word"
;
(word? first [~])
('scan-invalid = ((trap [load-value "~~"])/id))
(void? first [~~~])
('~ = label of '~~~)

; NULL is the response for when there is no content:
; https://forum.rebol.info/t/what-should-do-do/1426
;
(null? do [])
(
    foo: func [] []
    null? foo
)
(null? applique :foo [])
(null? do :foo)

; ~void~ is the convention for what you get by RETURN with no argument, or
; if the spec says <void> any result.
(
    foo: func [return: <void>] []
    '~void~ = foo
)(
    foo: func [] [return]
    '~void~ = foo
)
('~void~ = applique :foo [])
('~void~ = do :foo)
(
    data: [a b c]
    f: func [return: <void>] [append data [1 2 3]]
    '~void~ = f
)

; ~unset~ is the type of locals before they are assigned
(
    f: func [<local> loc] [get/any 'loc]
    f = '~unset~
)

; ~unset~ is also the type of things that just were never declared
(
    '~unset~ = get/any 'asiieiajiaosdfbjakbsjxbjkchasdf
)

; MATCH will match a void as-is, but falsey inputs produce ~matched~
[
    ('~preserved~ = match void! '~preserved~)
    ('~matched~ = match null null)
]

; CYCLE once differentiated a STOP result from BREAK with ~stopped~, but now
; it uses NULL-2 for similar purposes.
[
    (null-2 = cycle [stop])
    (null = cycle [break])
]

; ~quit~ is the label of the VOID! you get by default from QUIT
; Note: DO of BLOCK! does not catch quits, so TEXT! is used here.
[
    (1 = do "quit 1")
    ('~quit~ = do "quit")
    ('~unmodified~ = do "quit '~unmodified~")
]

; It's tougher to write generic routines that handle VOID! than to error on
; them, but a good general routine should probably do it.
;
([~abc~ ~def~] = collect [keep '~abc~, keep '~def~])

; Erroring modes of VOID! are being fetched by WORD! and logic tests.
; They are inert values otherwise, so PARSE should treat them such.
;
; !!! Review: PARSE should probably error on rules like `some ~foo~`, and
; there needs to be a mechanism to indicate that it's okay for a rule to
; literally match ~unset~ vs. be a typo.
;
(did parse [~foo~ ~foo~] [some '~foo~])  ; acceptable
(did parse [~foo~ ~foo~] [some ~foo~])  ; !!! shady, rethink
(
    foo: '~foo~
    e: trap [
        parse [~foo~ ~foo~] [some foo]  ; not acceptable  !!! how to overcome?
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


(error? trap [a: '~void~, a])
(not error? trap [set 'a '~void~])

(
    a-value: '~void~
    e: trap [a-value]
    e/id = 'need-non-void
)
