; %literal.test.reb
;
; Literal arguments allow callees the ability to distinguish the
; NULL, NULL-2, and END states.

[
    (did detector: func [@x [<opt> <end> any-value!]] [get/any 'x])

    ((just '10) = detector 10)
    (null = detector null)
    ((just ') = detector if true [null])

    ('~void~ = detector (comment "hi"))
    ('~void~ = detector)

    (did left-detector: enfixed :detector)

    ((just '1) = (1 left-detector))
    ('~void~ = left-detector)
    ('~void~ = (left-detector))
]

(
    x: false
    @(nihil) then [x: true]
    x
)
