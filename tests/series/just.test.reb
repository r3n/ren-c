; %just.test.reb
;
; JUST is a convenience designed to help with stricter rules about things like
; the requirement that blocks can only allow BLOCK!s and QUOTED!s to append
; to them.

((the 'x) = just x)
((the '[a b c]) = just [a b c])

([a b c d] = append [a b c] just d)
([a b c 'd] = append [a b c] just 'd)
([a b c [d e]] = append [a b c] just [d e])
([a b c '[d e]] = append [a b c] just '[d e])