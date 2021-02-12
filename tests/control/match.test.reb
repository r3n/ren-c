;
; %match.test.reb
;
; MATCH started out as a userspace function, but gained frequent enough use
; to become a native.  It is theorized that MATCH will evolve into the
; tool for checking arguments in function specs.
;

(10 = match integer! 10)
(null = match integer! "ten")

("ten" = match [integer! text!] "ten")
(20 = match [integer! text!] 20)
(null = match [integer! text!] <tag>)

(10 = match :even? 10)
(null = match :even? 3)


(void? match blank! _)
(null = match blank! 10)
(null = match blank! false)


; Quoting levels are taken into account with the rule, and the number of
; quotes is summed with whatever is found in the lookup.
;
; !!! because MATCH no longer quotes, you must use JUST
;
(just 'foo = match just 'word! just 'foo)
(null = match just 'word! just foo)

[
    (did quoted-word!: quote word!)

    (''foo = match ['quoted-word!] just ''foo)
    (null = match ['quoted-word!] just '''foo)
    ('''foo = match just '['quoted-word!] just '''foo)
]


; PATH! is AND'ed together, while blocks are OR'd
;
; !!! REVIEW: this is likely not the best idea, should probably be TUPLE!
; with generalized tuple mechanics.  Otherwise it collides with the inline
; MATCH experiment, e.g. `match parse/case "AAA" [some "A"]`.  But tuples
; are not generalized yet.

(1020 = match [integer!/[:even?]] 1020)
(null = match [integer!/[:odd?]] 304)
([a b] = match [block!/2 integer!/[:even?]] [a b])
(null = match [block!/3 integer!/[:even?]] null)
(304 = match [block!/3 integer!/[:even?]] 304)
(null = match [block!/3 integer!/[:even?]] 303)

(
    even-int: 'integer!/[:even?]
    just '304 = match just '[block!/3 even-int] just '304
)

; Falsey things are turned to VOID! in order to avoid cases like:
;
;     if match logic! flag [...]
;
; But can still be tested for value? since they are VOID!, and can be used
; with THEN and ELSE.
[
    (void? match null null)
    (void? match blank! blank)
    (true = match logic! true)
    (void? match logic! false)
]

[
    (10 = match integer! 10)
    (null = match integer! <tag>)

    ('a/b: = match any-path! 'a/b:)
    ('a/b: = match any-sequence! 'a/b:)
    (null = match any-array! 'a/b:)
]

; ENSURE is a version of MATCH that fails vs. returning NULL on no match
[
    (error? trap [ensure action! 10])
    (10 = ensure integer! 10)
]

; NON is an inverted form of ENSURE, that fails when the argument *matches*
[
    (error? trap [non action! :append])
    (10 = non action! 10)

    (error? trap [non integer! 10])
    (:append = non integer! :append)

    (10 = non null 10)
    (error? trap [non null null])
]


; MATCH was an early function for trying a REFRAMER-like capacity for
; building a frame of an invocation, stealing its first argument, and then
; returning that in the case of a match.  But now that REFRAMER exists,
; the idea of having that feature implemented in core functions has fallen
; from favor.
;
; Here we see the demo done with a reframer to make MATCH+ as a proof of
; concept of how it would be done if you wanted it.
[
    (match+: reframer func [f [frame!] <local> p] [
        p: f/(first parameters of action of f)  ; get the first parameter
        if do f [p]  ; evaluate to parameter if operation succeeds
    ]
    true)

    ; PARSE was a favored example because getting the PARSE input was useful.
    ; But now that's the default operation of PARSE.  Simulate a LOGIC!
    ; returning PARSE to show the benefit.
    ;
    (
        parse?: chain [:parse | :did]
        true
    )
    ("aaa" = match+ parse? "aaa" [some "a"])
    (null = match+ parse "aaa" [some "b"])
]


; Before REFRAMER existed, there was the concept of MAKE FRAME! on VARARGS!
; This is still a potentially useful operation.
;
; Test MAKE FRAME! from a VARARGS! with a test userspace implementation of the
; MATCH operation...
[
    (userspace-match: function [
        {Check value using tests (match types, TRUE or FALSE, or filter)}

        return: "Input if it matched, otherwise null (void if falsey match)"
            [<opt> any-value!]
        'args [<opt> any-value! <variadic>]
        'args-normal [<opt> any-value! <variadic>]
        <local> first-arg
    ][
        test: first args
        switch type of :test [
            word! path! [
                if action? get test [
                    f: make frame! args
                    first-arg: get in f first parameters of action of f
                    either-match false do f [return first-arg]
                    return null
                ]
            ]
        ]

        either-match :(take args) (take args-normal) @null
    ]
    true)

    (userspace-match integer! 10 then [true])
    (userspace-match integer! <tag> else [true])
    (10 = userspace-match even? 10)
    (null = userspace-match even? 7)
]
