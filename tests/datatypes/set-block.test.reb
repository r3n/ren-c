; SET-BLOCK! tests
;
; !!! For the most part, the functions %multi.test.reb covers SET-BLOCK!, as
; they are used for multiple return values.  This file should cover more
; scanner and mechanical tests of the type itself vs. that feature.

(set-block! = type of first [[a b c]:])
(set-path! = type of first [a/[b c d]:])
