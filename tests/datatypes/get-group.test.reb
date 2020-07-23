; GET-GROUP! tests

(get-group! = type of first [:(a b c)])
(get-path! = type of first [:(a b c)/d])

(
    m: 1020
    word: 'm
    :(word) = 1020
)

(
    o: make object! [f: 304]
    path: 'o/f
    :(path) = 304
)

(
    m: 1020
    o: make object! [f: 304]
    block: [m o/f]
    :(block) = :[m o/f]
)

; GET-GROUP! on arity-0 ACTION!s is also legal
(
    :(does [0]) = 0
)
