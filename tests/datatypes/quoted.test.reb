; Ren-C's QUOTED! is a generic and arbitrary-depth variant of the
; LIT-XXX! types from historical Rebol.
;
; SET and GET should see through escaping and work anyway

(
    unset 'a
    set just '''''a <seta>
    <seta> = get dequote just ''a
)(
    unset 'a
    set just 'a <seta>
    <seta> = get dequote just '''''''a
)(
    unset [a b]
    set reduce dequote ['''''a ''b] [<seta> <setb>]
    [<seta> <setb>] = get reduce [dequote 'a dequote '''''''b]
)

; Test basic binding, e.g. to make sure functions detect SET-WORD!

(
    x: 10
    set 'x: 20
    x = 20
)(
    x: 10
    y: _
    foo: function [] [
        set 'x: 20
        set 'y x
    ]
    foo
    (x = 10) and (y = 20)
)

; Try again, but set a QUOTED! (and not WORD! that results from literal)

(
    x: 10
    set just 'x: 20
    x = 20
)(
    x: 10
    y: _
    foo: function [] [
        set just 'x: 20
        set just 'y x
    ]
    foo
    (x = 10) and (y = 20)
)

; Now exceed the size of a literal that can be overlaid in a cell

(
    x: 10
    set just ''''''x: 20
    x = 20
)(
    x: 10
    y: _
    foo: function [] [
        set just '''''''x: 20
        set just '''''''y x
    ]
    foo
    (x = 10) and (y = 20)
)


; Deeply escaped words try to efficiently share bindings between different
; escapings.  But words in Rebol are historically atomic w.r.t. binding...
; doing a bind on a word returns a new word, vs. changing the binding of
; the word you put in.  Mechanically this means a changed binding must
; detach a deep literal from its existing cell and make new one.
(
    a: 0
    o1: make object! [a: 1]
    o2: make object! [a: 2]
    word: ''''''''''a:
    w1: bind word o1
    w2: bind word o2
    (0 = get dequote word) and (1 = get dequote w1) and (2 = get dequote w2)
)(
    foo: function [] [
        a: 0
        o1: make object! [a: 1]
        o2: make object! [a: 2]
        word: ''''''''''a:
        w1: bind word o1
        w2: bind word o2
        (0 = get dequote word) and (1 = get dequote w1) and (2 = get dequote w2)
    ]
    foo
)


(null? ')
(null? do ['])
(['] = reduce [''])
([''] = reduce ['''])
([' '' ''' ''''] = reduce ['' ''' '''' '''''])

(
    [1 (2 + 3) [4 + 5] a/+/b c/+/d: :e/+/f]
    = reduce
    ['1 '(2 + 3) '[4 + 5] 'a/+/b 'c/+/d: ':e/+/f]
)

(just '[a b c] = quote [a b c])
(just '(a b c) == quote just (a b c))
(not (just '[A B C] == quote [a b c]))
('''[a b c] !== '''''[a b c])
('''[a b c] == '''[a b c])
('''[a b c] = '''''[a b c])

; No quote levels is legal for QUOTE to add also, if /DEPTH is 0
[
    (null = quote/depth null 0)
    (<x> = quote/depth <x> 0)
]

(quoted! = kind of just 'foo)  ; low level "KIND"
((quote word!) = type of just 'foo)  ; higher-level "TYPE"
((type of just ''[a b c]) = quote/depth block! 2)


; REQUOTE is a reframing action that removes quoting levels and then puts
; them back on to the result.

((just ''''3) == requote add just ''''1 2)

((just '''[b c d]) == requote find ''''[a b c d] 'b)

(null == requote find ''''[a b c d] 'q)  ; nulls exempt

((just '(1 2 3 <four>)) == requote append ''(1 2 3) <four>)

('''a/b/c/d/e/f = requote join just '''a/b/c 'd/e/f)

((just '[1]) = (requote parse just '[1] [some integer!]))


; COPY should be implemented for all types, QUOTED! included.
;
((just '''[a b c]) == copy just '''[a b c])


; All escaped values are truthy, regardless of what it is they are escaping

(did just '_)
(did just '#[false])
(did just ')
(did just ''''''''_)
(did just ''''''''#[false])
(did just '''''''')


; An escaped word that can't fit in a cell and has to do an additional
; allocation will reuse that cell if it can (e.g. on each deliteralization
; step).  However, if that contains an ANY-WORD!, then a binding operation
; on that word will create a new cell allocation...similar to how bindings
; in LIT-WORD! could not be mutated, only create a new LIT-WORD!.
(
    a: 0
    o1: make object! [a: 1]
    o2: make object! [a: 2]
    word: '''''''''a
    w1: bind word o1
    w2: bind word o2
    did all [
        a = 0
        1 = get dequote w1
        2 = get dequote w2
    ]
)

; Smoke test for quoting items of every type

(
    for-each item compose [
        (:+)
        word
        set-word:
        :get-word
        /refinement
        #issue
        'quoted
        pa/th
        set/pa/th
        :get/pa/th
        (just (group))
        [block]
        #{AE1020BD0304EA}
        "text"
        %file
        e@mail
        <tag>
        (make bitset! 16)
        (make image! 10x20)
        (make vector! [integer! 32 100])
        (make map! [m a p !])
        (make varargs! [var args])
        (make object! [obj: {ect}])
        (make frame! :append)
        (make error! "error")
        (port: open http://example.com)
        #[true]
        10
        10.20
        10%
        $10.20
        #"a"
        10x20
        (make typeset! [integer! text!])
        (make gob! [])
        (make event! [type: 'done port: port])
        ("try handle here")
        ("try struct here")
        ("try library here")
        _
        |
        ~void~
    ][
        lit-item: quote get/any 'item

        comment "Just testing for crashes; discards mold result"
        mold :lit-item

        (e1: trap [equal1: equal? get/any 'item get/any 'item]) also [
            e1/where: e1/near: _
        ]
        (e2: trap [equal2: :lit-item = :lit-item]) also [
            e2/where: e2/near: _
        ]
        if e1 [e1/line: null]  ; ignore line difference (file should be same)
        if e2 [e2/line: null]
        if :e1 != :e2 [
            print mold type of get/any 'item
            print mold e1
            print mold e2
            fail "no error parity"
        ]
        if equal1 != equal2 [
            fail "no comparison parity"
        ]
    ]
    close port
    true
)


(
    did all [
        void? x: '~void~
        void? get/any 'x
    ]
)
