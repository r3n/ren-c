; datatypes/none.r
(blank? blank)
(not blank? 1)
(blank! = type of blank)
; literal form
(blank = _)
[#845
    (blank = _)
]

; MAKE of a BLANK! is not legal, but MAKE TARGET-TYPE BLANK follows the rule
; of "blanks in, nulls out".  (If it tried to do a custom error at the moment,
; that would lose the efficiency of the optimized handling where revoked
; blanks don't run the functions body at all.)
;
(null = make blank! blank)
(error? trap [make blank! [a b c]])
(null = make integer! blank)
(null = make object! blank)

(null? to blank! _)  ; TO's universal protocol for blank 2nd argument
(null? to _ 1)  ; TO's universal protocol for blank 1st argument
(error? trap [to blank! 1])  ; no other types allow "conversion" to blank

("_" = mold blank)
[#1666 #1650 (
    f: does [_]
    _ == f
)]
