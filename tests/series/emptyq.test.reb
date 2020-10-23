; functions/series/emptyq.r
(empty? [])
(
    blk: tail of [1]
    clear head of blk
    not empty? blk  ; !!! currently answers as "not tail?" so not empty
)
(empty? blank)
[#190
    (x: copy "xx^/" loop 20 [enline y: join x x] true)
]
