; The SYM-GROUP! type is new and needs testing.

(sym-group! = type of '^(a b c))

; ENDs make unfriendly voids when literalized, that if you further literalize
; will make friendly ones.
[
    ('~void~ = ^ ^())
    ('~void~ = ^ ^(comment "hi"))
    ('~void~ = ^ ^(void))

    ('~void~ = friendly ^())
    ('~void~ = friendly ^(comment "hi"))
    ('~void~ = friendly ^(void))
]

((the '10) = ^(10 comment "hi"))

(null = ^(null))
((the ') = ^(if true [null]))

((the '1020) = ^(1000 + 20))
