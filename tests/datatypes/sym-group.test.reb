; The SYM-GROUP! type is new and needs testing.

(sym-group! = type of '@(a b c))

('~void~ = @())
('~void~ = @(comment "hi"))
('~void~ = @(nihil))
((just '10) = @(10 comment "hi"))

(null = @(null))
((just ') = @(if true [null]))

((just '1020) = @(1000 + 20))
