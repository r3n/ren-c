; system/gc.r

[#1776 #2072 (
    a: copy []
    loop 200'000 [a: append/only copy [] a]
    recycle
    true
)]
[#1989 (
    loop ([comment 30000000] 300) [make gob! []]
    true
)]

; !!! simplest possible LOAD/SAVE smoke test, expand!
(
    file: %simple-save-test.r
    data: "Simple save test produced by %core-tests.r"
    save file data
    (load file) = data
)


;
; "Mold Stack" tests
;

; Nested unspaced
(
    nested-unspaced: func [n] [
        either n <= 1 [n] [unspaced [n _ nested-unspaced n - 1]]
    ]
    "9 8 7 6 5 4 3 2 1" = nested-unspaced 9
)
; Form recursive object...
(
    o: construct [a: 1 r: _] o/r: o
    (unspaced ["<" form o  ">"]) = "<a: 1^/r: make object! [...]>"
)
; detab...
(
    (unspaced ["<" detab "aa^-b^-c" ">"]) = "<aa  b   c>"
)
; entab...
(
    (unspaced ["<" entab "     a    b" ">"]) = "<^- a    b>"
)
; dehex...
(
    (unspaced ["<" dehex "a%20b" ">"]) = "<a b>"
)
; form...
(
    (unspaced ["<" form [1 <a> [2 3] "^""] ">"]) = {<1 <a> 2 3 ">}
)
; transcode...
(
    (unspaced ["<" mold transcode to binary! "a [b c]" ">"])
        = "<[a [b c]]>"
)
; ...
(
    (unspaced ["<" intersect [a b c] [d e f]  ">"]) = "<>"
)
