; datatypes/set-path.r
(set-path? first [a/b:])
(not set-path? 1)
(set-path! = type of first [a/b:])
; the minimum
[#1947
    (set-path? load-value "#[set-path! [[a] 1]]")
]

; ANY-PATH! are no longer positional
;
;(
;    all [
;        set-path? a: load-value "#[set-path! [[a b c] 2]]"
;        2 == index? a
;    ]
;)

("a/b:" = mold first [a/b:])
; set-paths are active
(
    a: make object! [b: _]
    a/b: 5
    5 == a/b
)
[#1 (
    o: make object! [a: 0x0]
    o/a/x: 71830
    o/a/x = 71830
)]
; set-path evaluation order
(
    a: 1x2
    a/x: (a: [x 4] 3)
    any [
        a == 3x2
        a == [x 3]
    ]
)
[#64 (
    blk: [1]
    i: 1
    blk/:i: 2
    blk = [2]
)]


; Typically SET and GET evaluate GROUP!s in paths, vs. treat them "as-is".
; But various circumstances like using GROUP! as map keys warrant it.  It's
; also used to avoid double-evaluations in things like DEFAULT (so it can
; COMPOSE a SET-PATH! once, then use GET/HARD to see if it has a value and
; SET/HARD to assign it if needed.)
(
   counter: 0
   obj: make object! [x: _]
   obj/(counter: counter + 1 'x): default [<thing>]
   did all [
       obj/x = <thing>
       counter = 1
   ]
)(
    m: make map! 10
    set/hard 'm/(1 + 2) <hard>
    did all [
        <hard> = pick m lit (1 + 2)
        <hard> = get/hard 'm/(1 + 2)
    ]
)


