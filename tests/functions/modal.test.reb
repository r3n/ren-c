; Modal parameters are described here:
; https://forum.rebol.info/t/1187

[
    "Basic operational test"

    (did foo: function [@x /y] [
        reduce .voidify [x y]
    ])

    ([3 #[void]] = foo 3)
    ([3 /y] = foo @(1 + 2))
    ([@(1 + 2) #[void]] = foo '@(1 + 2))

    (did item: 300)

    ([304 #[void]] = foo item + 4)
    ([304 /y] = foo @(item + 4))
    ([@(item + 4) #[void]] = foo '@(item + 4))

    ([300 #[void]] = foo item)
    ([300 /y] = foo @item)
    ([@item #[void]] = foo '@item)

    ([[a b] #[void]] = foo [a b])
    ([[a b] /y] = foo @[a b])
    ([@[a b] #[void]] = foo '@[a b])

    (did obj: make object! [field: 1020])

    ([1020 #[void]] = foo obj/field)
    ([1020 /y] = foo @obj/field)
    ([@obj/field #[void]] = foo '@obj/field)
]

[
    "Basic infix operational test"

    (did bar: enfix function [@x /y] [
        reduce .voidify [x y]
    ])

    (3 bar = [3 #[void]])
    (@(1 + 2) bar = [3 /y])

    (did item: 300)

    ((item + 4) bar = [304 #[void]])
    (@(item + 4) bar = [304 /y])

    (item bar = [300 #[void]])
    (@item bar = [300 /y])

    ([a b] bar = [[a b] #[void]])
    (@[a b] bar = [[a b] /y])

    (did obj: make object! [field: 1020])

    (obj/field bar = [1020 #[void]])
    (@obj/field bar = [1020 /y])
]

[
    "Demodalizing specialization test"

    (did foo: function [a @x /y] [
        reduce .voidify [a x y]
    ])

    ([a @x /y] = parameters of :foo)

    ([10 20 #[void]] = foo 10 20)
    ([10 20 /y] = foo 10 @(20))

    (did fooy: :foo/y)

    ([a x] = parameters of :fooy)
    ([10 20 /y] = fooy 10 20)
    (
        'bad-refine = (trap [
            fooy/y 10 20
        ])/id
    )
    (
        'bad-refine = (trap [
            fooy 10 @(20)
        ])/id
    )
]

; Invisibility sensitivity
;
; Modal parameters use the unevaluated flag to inform callers that an
; argument "dissolved", so they can differentiate @(comment "hi") and @(null)
; The mechanism used is much like how <end> and <opt> are distinguished.
[
    (sensor: func [@arg [<opt> any-value!] /modal] [
        reduce .try [arg modal semiquoted? 'arg]
    ] true)

    ([_ /modal #[false]] = sensor @(null))
    ([_ /modal #[true]] = sensor @())
    ([_ /modal #[true]] = sensor @(comment "hi"))

    ; ([_ /modal #[true]] = sensor @nihil)  ; !!! maybe this should work?
]
