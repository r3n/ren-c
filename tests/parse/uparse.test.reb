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


; SOME and ANY have become value-bearing; they give back blocks if they match.
[(
    x: _
    did all [
        uparse "aaa" [x: any "a"]
        x = ["a" "a" "a"]
    ]
)(
    x: _
    did all [
        uparse "aaa" [x: any "b", any "a"]
        x = []
    ]
)(
    x: _
    did all [
        uparse "aaa" [x: opt some "b", any "a"]
        x = null
    ]
)(
    x: _
    did all [
        uparse "aaa" [x: opt some "a"]
        x = ["a" "a" "a"]
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
             left: some "<"
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
; up objects.  It works with a GATHER and SET-WORD! to capture into a
; variable, but even goes so far as to make it so the overall parse result
; can be changed to an object if you use it.
;
; !!! That might be a lame idea, revisit.
[(
    obj: uparse [1 <foo>] [emit i: integer!, emit t: tag!]
    did all [
        obj/i = 1
        obj/t = <foo>
    ]
)(
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
        uparse [| | any any any | | |] [
            content: between some '| some '|
            into @content [x: some 'any]
        ]
        x = [any any any]
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

; !!! Mixing SET-WORD! and BLOCK! is currently up in the air, in terms of
; exactly how it should work.
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
        x = [1 "hello"]
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
        uparse [1 2 3] [x: [some integer!]]
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
