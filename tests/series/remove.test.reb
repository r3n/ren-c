; functions/series/remove.r
[
    ([] = remove [])
    ([] = head of remove [1])
    ([2] = head of remove [1 2])
]

[
    ("" = remove "")
    ("" = head of remove "1")
    ("2" = head of remove "12")
]

[
    (#{} = remove #{})
    (#{} = head of remove #{01})
    (#{02} = head of remove #{0102})
]

; bitset
(
    a-bitset: charset "a"
    remove/part a-bitset "a"
    null? find a-bitset #"a"
)
(
    a-bitset: charset "a"
    remove/part a-bitset to integer! #"a"
    null? find a-bitset #"a"
)

[
    (1 = take #{010203})
    (#{01} = take/part #{010203} 1)  ; always a series
    (3 = take/last #{010203})
    (#{0102} = take/part #{010203} 2)
    (#{0203} = take/part next #{010203} 100)  ; should clip

    (#"a" = take "abc")
    ("a" = take/part "abc" 1)  ; always a series
    (#"c" = take/last "abc")
    ("ab" = take/part "abc" 2)
    ("bc" = take/part next "abc" 100)  ; should clip

    ('a = take [a b c])
    ([a] = take/part [a b c] 1)  ; always a series
    ('c = take/last [a b c])
    ([a b] = take/part [a b c] 2)
    ([b c] = take/part next [a b c] 100)  ; should clip
]
