;
; MATCH started out as a userspace function, but gained frequent usage...and
; also has an interesting variadic feature that is not (yet) available to
; userspace VARARGS!.  This enables it to intercept the first argument of a
; filter function, that is invoked.
;
; https://github.com/metaeducation/ren-c/pull/730
;

("aaa" = match parse "aaa" [some "a"])
(null = match parse "aaa" [some "b"])

(10 = match integer! 10)
(null = match integer! "ten")

("ten" = match [integer! text!] "ten")
(20 = match [integer! text!] 20)
(null = match [integer! text!] <tag>)

(10 = match :even? 10)
(null = match :even? 3)

; !!! MATCH is a tricky action that quotes its first argument, -but- if it
; is a word that calls an action, it builds a frame and invokes that action.
; It's taking on some of the responsibility of the evaluator, and is hence
; experimental and problematic.  Currently we error on quoted WORD!s, until
; such time as the feature is thought out more to know exactly what it
; should do...as it wouldn't see the quote if it were thought of as eval'ing.
;
; (null = match 'odd? 20)
; (7 = match 'odd? 7)

(void? match blank! _)
(null = match blank! 10)
(null = match blank! false)


; Quoting levels are taken into account with the rule, and the number of
; quotes is summed with whatever is found in the lookup.

(lit 'foo = match 'word! lit 'foo)
(null = match 'word! lit foo)

[
    (did quoted-word!: quote word!)

    (''foo = match ['quoted-word!] lit ''foo)
    (null = match ['quoted-word!] lit '''foo)
    ('''foo = match '['quoted-word!] lit '''foo)
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
    lit '304 = match '[block!/3 even-int] lit '304
)
