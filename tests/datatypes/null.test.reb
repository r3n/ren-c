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

; There are two "isotopes" of NULL (NULL-1 and NULL-2).
; Both answer to being NULL?  A variable assigned with NULL-2 will decay
; to a regular NULL when accessed via a WORD!/GET-WORD!/etc.
;
; The specific role of NULL-2 is to be reactive with THEN and not ELSE, so
; that branches may be purposefully NULL.
[
    (null? null)
    (null? heavy null)
    (null-1? null)
    (heavy-null? heavy null)

    (x: heavy null, null-1? x)
    (x: heavy null, null-1? :x)

    (304 = (null then [1020] else [304]))
    (1020 = (heavy null then [1020] else [304]))
]

; Conditionals return NULL-1 on failure, and NULL-2 on a branch that executes
; and evaluates to either NULL-1 or NULL-2.  If the branch wishes to pass
; the null "as-is" it should use the @ forms.
[
    (heavy-null? if true [null])
    (heavy-null? if true [heavy null])
    ('~void~ = @ if true [])
    ('~custom~ = @ if true [~custom~])
    (''~custom~ = @ if true ['~custom~])

    (null-1? if true @[null])
    (heavy-null? if true @[heavy null])
    ('~void~ = @ if true @[])
    ('~custom~ = @ if true @[~custom~])
    (''~custom~ = @ if true @['~custom~])
]
