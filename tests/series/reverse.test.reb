; functions/series/reverse.r
[#1810 ; REVERSE/part does not work for tuple!
    (3.2.1.4.5 = reverse/part 1.2.3.4.5 3)
]


[#2326 (
    data: collect [keep/line [1 2] keep/line [3 4 5] keep/line [6]]
    ; == [
    ;     1 2
    ;     3 4 5
    ;     6
    ; ]
    before: collect [
        for-next pos data [keep new-line? pos]
        keep new-line? tail data
    ]

    reverse data
    ; == [
    ;     6
    ;     5 4 3
    ;     2 1
    ; ]
    after: collect [
        for-next pos data [keep new-line? pos]
        keep new-line? tail data
    ]

    did all [
        before = [#[true] #[false] #[true] #[false] #[false] #[true] #[true]]
        after = [#[true] #[true] #[false] #[false] #[true] #[false] #[true]]
    ]
)]
