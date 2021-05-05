; %literal.test.reb
;
; Literal arguments allow callees the ability to distinguish the
; NULL, NULL-2, and END states.

[
    (did detector: func [@x] [get/any 'x])

    ((just '10) = detector 10)
    (null = detector null)
    ((just ') = detector if true [null])
    ('~invisible~ = detector (comment "hi"))
]
