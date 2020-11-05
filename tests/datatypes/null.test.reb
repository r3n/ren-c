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
; Responsibility for kind of "ornery-ness" this shifted to VOID!, as NULL
; took on increasing roles as the "true NONE!" and became the value for
; unused refinements.
;
(null? trap [a: null a])
(not error? trap [set 'a null])

; NULL has no visual representation, so MOLD and FORM errors on it
; Users are expected to triage the NULL to vaporize, error, or find some
; way to represent it "out of band" in their target medium.
(
    e: trap [mold null]
    'arg-required = e/id
)
(
    e: trap [form null]
    'arg-required = e/id
)
