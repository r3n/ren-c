; %uparse.test.reb
;
; UPARSE for (U)sermode PARSE is a new architecture for PARSE based on the
; concept of parser combinators.  The goal is to put together a more coherent
; and modular design in usermode, which can then be extended or altered by
; using a different set of combinators.  Those combinators can be chosen for
; new features, or just to get compatibility with Rebol2/R3-Alpha/Red parse.

; SYM-XXX! are value-bearing rules that do not advance the input and get their
; argument literally.  They succeed unless their result is null (if you want
; success in that case with a null result, combine with OPT rule).
[(
    three: 3
    did all [
        "" = uparse "" [x: @three]
        x = 3
    ]
)(
    did all [
        "" = uparse "" [x: @(1 + 2)]
        x = 3
    ]
)(
    x: <before>
    did all [
        not uparse "" [x: @(null)]
        x = <before>
    ]
)(
    x: <before>
    did all [
        "" = uparse "" [x: opt @(null)]
        x = null
    ]
)]


; SOME and WHILE have become value-bearing; giving back blocks if they match.
[(
    x: _
    did all [
        uparse "aaa" [x: while "a"]
        x = "a"
    ]
)(
    x: _
    did all [
        uparse "aaa" [x: while "b", while "a"]
        x = null
    ]
)(
    x: _
    did all [
        uparse "aaa" [x: opt some "b", while "a"]
        x = null
    ]
)(
    x: _
    did all [
        uparse "aaa" [x: opt some "a"]
        x = "a"
    ]
)]


; A TEXT! rule will capture the actual match in a block.  But for a string, it
; will capture the *rule*.
[(
    rule: [x: "a"]
    did all [
        uparse "a" rule
        same? x second rule
    ]
)(
    data: ["a"]
    rule: [x: "a"]
    did all [
        uparse data rule
        same? x first data
    ]
)]


; One key feature of UPARSE is that rule chaining is done in such a way that
; it delegates the recognition to the parse engine, meaning that rules do not
; have to be put into blocks as often.
[
    (["aaaa"] = uparse ["aaaa"] [into text! some 2 "a"])
    (null = uparse ["aaaa"] [into text! some 3 "a"])
]

; BETWEEN is a new combinator that lets you capture between rules.
[(
    did all [
        uparse "aaaa(((How cool is this?))aaaa" [
            some "a", x: between some "(" some ")", some "a"
        ]
        x = "How cool is this?"
    ]
)(
    did all [
        uparse [<a> <b> * * * {Thing!} * * <c>] [
            some tag!, x: between 3 '* 2 '*, some tag!
        ]
        x = [{Thing!}]
    ]
)]

; SET-WORD! rules that do not match should not disrupt the variable, but if
; OPT is used with it then that indicates it should be set to NULL.
[(
    t: "t"
    i: "i"
    did all [
        uparse [<foo>] [i: integer! | t: tag!]
        i = "i"  ; undisturbed
        t = <foo>
    ]
)(
    t: "t"
    i: "i"
    did all [
        uparse [<foo>] [i: opt integer!, t: tag!]
        i = null
        t = <foo>
    ]
)]

; If a DATATYPE! is used in a text or binary rule, that is interpreted as a
; desire to TRANSCODE the input.
;
; !!! This feature needs more definition, e.g. to be able to transcode things
; that don't end with space or end of input.  For instance, think about how
; to handle the below rule if it was `1020****` and having a `some "*"` rule
; at the tail as well.
(
    did all [
        uparse "***{A String} 1020" [some "*", t: text!, i: integer!]
        t = {A String}
        i = 1020
    ]
)

; HERE follows Topaz precedent as the new means of capturing positions
; (e.g. POS: HERE).  But it is useful for other purposes, when a rule is
; needed for capturing the current position.
[(
    did all [
        parse "aaabbb" [some "a", pos: here, some "b"]
        pos = "bbb"
    ]
)(
     did all [
         uparse "<<<stuff>>>" [
             left: across some "<"
             (n: length of left)
             x: between here n ">"
         ]
         x = "stuff"
     ]
)]

; COLLECT is currently implemented to conspire with the BLOCK! combinator to
; do rollback between its alternates.  But since anyone can write combinators
; that do alternates (in theory), those would have to participate in the
; protocol of rollback too.
;
; Test the very non-generic rollback mechanism.  The only two constructs that
; do any rollback are BLOCK! and COLLECT itself.
[(
    x: <before>
    did all [
        uparse [1 2] [x: collect [
            keep integer! keep tag! | keep integer! keep integer!
        ]]
        x = [1 2]
    ]
)(
    x: <before>
    did all [  ; semi-nonsensical use of BETWEEN just because it takes 2 rules
        uparse "(abc)" [x: collect between keep "(" keep ")"]
        x = ["(" ")"]
    ]
)(
    x: <before>
    did all [  ; semi-nonsensical use of BETWEEN just because it takes 2 rules
        not uparse "(abc}" [x: collect between "(" keep ")"]
        x = <before>
    ]
)(
    x: <before>
    did all [
        uparse "aaa" [x: collect [some [
            keep opt @(if false [<not kept>])
            keep skip
            keep @(if true [<kept>])
        ]]]
        x = [#a <kept> #a <kept> #a <kept>]
    ]
)]

; EMIT is a new idea to try and make it easier to use PARSE rules to bubble
; up objects.  It works with a GATHER and SET-WORD!
[(
    uparse [* * * 1 <foo> * * *] [
        some '*
        g: gather [
            emit i: integer!, emit t: text! | emit i: integer!, emit t: tag!
        ]
        some '*
    ]
    did all [
        g/i = 1
        g/t = <foo>
    ]
)(
    let result
    uparse "aaabbb" [
       result: gather [
            emit x: collect some ["a", keep @(<a>)]
            emit y: collect some ["b", keep @(<b>)]
       ]
    ] else [
       fail "Parse failed"
    ]
    did all [
        result.x = [<a> <a> <a>]
        result.y = [<b> <b> <b>]
    ]
)]

; If you EMIT with no GATHER, the current behavior is to make the UPARSE
; itself emit variable definitions, much like LET.  Having this be a feature
; of EMIT instead of a new keyword might not be the best idea, but it's
; being tried out for now.
[(
    i: #i
    t: #t
    if true [
        uparse [1 <foo>] [emit i: integer!, emit t: tag!]
        assert [i = 1, t = <foo>]
    ]
    did all [
        i = #i
        t = #t
    ]
)(
    base: #base
    extension: #extension
    if true [
       let filename: "demo.txt"
       uparse filename [
            emit base: between here "."
            emit extension: thru end
        ] else [
            fail "Not a file with an extension"
        ]
        assert [base = "demo"]
        assert [extension = "txt"]
    ]
    did all [
        base = #base
        extension = #extension
    ]
)]

; UPARSE INTO is arity-2, permitting use of a value-bearing rule to produce
; the thing to recurse the parser into...which can generate a new series, as
; well as pick one out of a block.
[(
    did all [
        uparse ["aaa"] [into text! [x: across some "a"]]
        x = "aaa"
    ]
)(
    did all [
        uparse ["aaa"] [into skip [x: across some "a"]]
        x = "aaa"
    ]
)(
    did all [
        uparse "((aaa)))" [
            into [between some "(" some ")"] [x: across some "a"]
        ]
        x = "aaa"
    ]
)(
    did all [
        uparse [| | while while while | | |] [
            content: between some '| some '|
            into @content [x: collect [some keep 'while]]
        ]
        x = [while while while]
    ]
)]

; CHANGE is rethought in UPARSE to work with value-bearing rules.  The rule
; gets the same input that the first argument did.
;
[(
    str: "aaa"
    did all [
        uparse str [change [some "a"] @(if true ["literally"])]
        str = "literally"
    ]
)(
    str: "(aba)"
    did all [
        uparse str [
            "("
            change [to ")"] [
                collect [
                    some ["a" keep @("A") | skip]
                ]
            ]
            ")"
        ]
        str = "(AA)"
    ]
)]

; Mixing SET-WORD! with block returns the last value-bearing rule in the PARSE
; (Note: more on this later in this file; review when breaking out separate
; tests for UPARSE into separate files.)
[(
    x: <before>
    did all [
        uparse [1 "hello"] [x: [tag! | integer!] text!]
        x = 1  ; not [1]
    ]
)(
    x: <before>
    did all [
        uparse [1 "hello"] [x: [tag! integer! | integer! text!]]
        x = "hello"
    ]
)(
    x: <before>
    did all [
        uparse [] [x: [opt integer!]]
        x = null
    ]
)(
    x: <before>
    did all [
        not uparse [] [x: [integer!]]
        x = <before>
    ]
)(
    x: <before>
    did all [
        uparse [] [x: opt [integer!]]
        x = null
    ]
)

(
    did all [
        uparse [1 2 3] [x: collect [some keep integer!]]
        x = [1 2 3]
    ]
)]

; SOME can call generators, terminating on the null
[(
    gen: func [<static> n (0)] [
        if n < 3 [return n: n + 1]
        return null
    ]

    did all [
        "a" = uparse "a" ["a", data: some @(gen)]
        data = [1 2 3]
    ]
)]

; RETURN was removed from Ren-C PARSE but is being re-added to UPARSE, as
; it seems useful enough to outweigh the interface complexity.
[(
    10 = uparse [aaa] [return @(10)]
)(
    let result: uparse "aaabbb" [
        return gather [
            emit x: collect some ["a", keep @(<a>)]
            emit y: collect some ["b", keep @(<b>)]
        ]
    ] else [
        fail "Parse failed"
    ]
    did all [
        result.x = [<a> <a> <a>]
        result.y = [<b> <b> <b>]
    ]
)]

; NOT NOT should be equivalent to AHEAD
; Red at time of writing has trouble with this
; As does Haskell Parsec, e.g. (notFollowedBy . notFollowedBy != lookAhead)
; https://github.com/haskell/parsec/issues/8
[
    ("a" = uparse "a" [[not not "a"] "a"])
]

[(
    x: uparse "baaabccc" [into [between "b" "b"] [some "a" end] to end]
    x == "baaabccc"
)(
    x: uparse "baaabccc" [into [between "b" "b"] ["a" end] to end]
    x = null
)(
    x: uparse "baaabccc" [into [between "b" "b"] ["a"] to end]
    x = null
)(
    x: uparse "baaabccc" [into [between "b" "b"] ["a" to end] "c" to end]
    x = "baaabccc"
)(
    x: uparse "aaabccc" [into [across to "b"] [some "a"] to end]
    x = "aaabccc"
)]

; INTO can be mixed with HERE to parse into the same series
[(
    x: uparse "aaabbb" [
        some "a"
        into here ["bbb" (b: "yep, Bs")]
        "bbb" (bb: "Bs again")
    ]
    did all [
        x = "aaabbb"
        b = "yep, Bs"
        bb = "Bs again"
    ]
)(
    x: uparse "aaabbbccc" [
        some "a"
        into here ["bbb" to end (b: "yep, Bs")]
        "bbb" (bb: "Bs again")
        "ccc" (c: "Here be Cs")
    ]
    did all [
        x = "aaabbbccc"
        b = "yep, Bs"
        bb = "Bs again"
        c = "Here be Cs"
    ]
)]

; TO and THRU are now value bearing, e.g. `x: thru "a"` acts as what would
; have historicaly been `copy x thru "a"`.
[(
    "aaab" = uparse "aaabbb" [return thru "b"]
)(
    "aaa" = uparse "aaabbb" [return to "b"]
)]

; GET-GROUP!s will splice rules, null means no rule but succeeds...FALSE is
; useful for failing, and TRUE is a synonym for NULL in this context.

[
    ("aaa" = uparse "aaa" [:(if false ["bbb"]) "aaa"])
    ("bbbaaa" = uparse "bbbaaa" [:(if true ["bbb"]) "aaa"])
]

; a BLOCK! rule combined with SET-WORD! will evaluate to the last value-bearing
; result in the rule.  This provides compatibility with the historical idea
; of doing Redbol rules like `set var [integer! | text!]`, but in that case
; it would set to the first item captured from the original input...more like
; `copy data [your rule], (var: first data)`.
;
; https://forum.rebol.info/t/separating-parse-rules-across-contexts/313/6
[
    (2 = uparse [1 2] [return [integer! integer!]])
    ("a" = uparse ["a"] [return [integer! | text!]])
]

; A BLOCK! rule is allowed to return NULL, but this is a bit confusing since
; invisible rules return NULL too...so initializing to null may not be the
; best.  Reconsider void! behaviors, e.g. should `x: []` give back a void...
; or maybe even error forcing you to say `x.: []` or some other override if
; you really meant to take the void?
[
    (
        x: <before>
        did all [
            uparse [1] [x: [integer! opt text!]]
            x = null
        ]
    )

    (
        x: <before>
        did all [
            uparse [1] [integer! x: [elide end]]
            x = null
        ]
    )
]
