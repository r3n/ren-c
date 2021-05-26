; %lit.test.reb
;
; Operator whose evaluator behavior is like QUOTE, but it distinguishes
; the internal NULL-2 state from plain NULL (which quote does not)

((the '3) = ^ 1 + 2)

((the ') = ^ if true [null])

(null = ^ null)

; The ^ does not subvert normal "right hand side evaluation rules", and
; as such it skips invisibles, vs. giving back ~void~.  Use the ^(...) in order
; to detect invisibility.

((the '3) = ^ comment "Hi" 1 + 2)
(
    e: trap [^]
    e.id = 'need-non-end
)
