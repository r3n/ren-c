; null.test.reb
;
; Note: NULL is not technically a datatype.
;
; It is a transitional state that can be held by variables, but it cannot
; appear in BLOCK!s etc.
;
; While it might be thought of as "variables can hold NULL", it's better
; to say that "NULL is a transitional result used in the evaluator that
; is used by GET to convey a variable is *not set*".  This is a small point
; of distinction, but it helps understand NULL better.

(null? null)
(null? type of null)
(not null? 1)

; Early designs for NULL did not let you get or set them from plain WORD!
; Responsibility for kind of "ornery-ness" this shifted to BAD-WORD!, as NULL
; took on increasing roles as the "true NONE!" and became the value for
; unused refinements.
;
(null? trap [a: null a])
(not error? trap [set 'a null])

; NULL has no visual representation, so FORM errors on it
; Users are expected to triage the NULL to vaporize, error, or find some
; way to represent it "out of band" in their target medium.
;
; MOLD was reviewed and deemed to be better without this protection than with.
; (It may be reviewed an decided the same for FORM)
(
    null = mold null
)
(
    e: trap [form null]
    'arg-required = e/id
)

; Both the ~null~ isotope and "true" null answer to being NULL? (the isotope
; decays in normal parameters, so the NULL? function doesn't know the
; difference).  A variable assigned with a ~null~ isotope will decay to a
; regular ~null~ when accessed via a WORD!/GET-WORD!/etc.
;
; The specific role of ~null~ isotopes is to be reactive with THEN and not
; ELSE, so that failed branches may be purposefully NULL.
[
    (null = ^ null)
    (null? heavy null)
    ((the ') = ^ heavy null)

    (x: heavy null, null = ^ x)
    (x: heavy null, null = ^ :x)

    (304 = (null then [1020] else [304]))
    (1020 = (heavy null then [1020] else [304]))
]

; Conditionals return NULL on failure, and ~null~ isotope on a branch that
; executes and evaluates to either NULL or ~null~ isotope.  If the branch
; wishes to pass the null "as-is" it should use the ^ forms.
[
    ((the ') = ^ if true [null])
    ((the ') = ^ if true [heavy null])
    ('~void~ = ^ if true [])
    ('~custom~ = ^ if true [~custom~])
    (''~custom~ = ^ if true ['~custom~])

    (null = ^ if true ^[null])
    ((the ') = ^ if true ^[heavy null])
    ('~void~ = ^ if true ^[])
    ('~custom~ = ^ if true ^[~custom~])
    (''~custom~ = ^ if true ^['~custom~])
]
