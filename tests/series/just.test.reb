; %just.test.reb
;
; Creates optimized shallowly immutable single-element block.
; Further optimizations planned:
;
; https://forum.rebol.info/t/1182/13

([] = just null)
([1] = just 1)
([[1]] = just just 1)

; Top-level immutability
(
    block: [x]
    j: just block
    'series-frozen = (trap [append j <illegal>])/id
)

; If contents were mutable, they still will be if contained
(
    block: [x]
    j: just block
    append first j <legal>
    [[x <legal>]] = j
)
