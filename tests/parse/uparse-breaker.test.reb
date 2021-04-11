; uparse-breaker.test.reb

[
    (did breaker: func [text] [
        let capturing
        let inner
        return uparse text [return collect [any [
            not end
            (capturing: false)
            keep opt between here ["$(" (capturing: true) | end]
            :(if capturing '[
                inner: between here ")"
                keep @(as word! inner)
            ])
        ]]]
    ])

    (["abc" def "ghi"] = breaker "abc$(def)ghi")
    ([] = breaker "")
    (["" abc "" def "" ghi] = breaker "$(abc)$(def)$(ghi)")
]
