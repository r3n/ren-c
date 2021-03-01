; %uparse.test.reb
;
; UPARSE for (U)sermode PARSE is a new architecture for PARSE based on the
; concept of parser combinators.  The goal is to put together a more coherent
; and modular design in usermode, which can then be extended or altered by
; using a different set of combinators.  Those combinators can be chosen for
; new features, or just to get compatibility with Rebol2/R3-Alpha/Red parse.

; One key feature of UPARSE is that rule chaining is done in such a way that
; it delegates the recognition to the parse engine, meaning that rules do not 
; have to be put into blocks as often.
[
    (["aaaa"] = uparse ["aaaa"] [into some 2 "a"])
    (null = uparse ["aaaa"] [into some 3 "a"])
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
