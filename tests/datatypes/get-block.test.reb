; GET-BLOCK! tests

(get-block! = type of first [:[a b c]])
(get-path! = type of first [:[a b c]/d])

(
    a: 10 b: 20
    :[a b] = [a b]
)
