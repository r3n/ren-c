; %frame.test.reb
;
; The FRAME! type is foundational to the mechanics of Ren-C vs. historical
; Rebol.  While its underlying storage is similar to an OBJECT!, it has a
; more complex mechanic based on being able to be seen through the lens of
; multiple different "views" based on which phase of a function composition
; it has been captured for.


; Due to some fundamental changes to how parameter lists work, such that
; each action doesn't get its own copy, some tests of parameter hiding no
; longer work.  They were moved into this issue for consideration:
;
; https://github.com/metaeducation/ren-c/issues/393#issuecomment-745730620

; An "original" FRAME! that is created is considered to be phaseless.  When
; you execute that frame, the function will destructively use it as the
; memory backing the invocation.  This means it will treat the argument cells
; as if they were locals, manipulating them in such a way that the frame
; cannot meaningfully be used again.  The system flags such frames so that
; you don't accidentally try to reuse them or assume their arguments can
; act as caches of the input.
(
    f: make frame! :append
    f/series: [a b c]
    f/value: <d>
    did all [
        [a b c <d>] = do copy f  ; making a copy works around the expiration
        f/series = [a b c <d>]
        f/value = <d>
        [a b c <d> <d>] = do f
        'stale-frame = pick trap [do f] 'id
        'stale-frame = pick trap [f/series] 'id
        'stale-frame = pick trap [f/value] 'id
    ]
)
