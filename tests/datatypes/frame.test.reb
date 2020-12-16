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
