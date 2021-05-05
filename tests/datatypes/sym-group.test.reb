; The SYM-GROUP! type is new and needs testing.

(sym-group! = type of '@(a b c))

('~invisible~ = @())
('~invisible~ = @(comment "hi"))
('~invisible~ = @(nihil))
((just '10) = @(10 comment "hi"))

(null = @(null))
((just ') = @(if true [null]))

((just '1020) = @(1000 + 20))
