; Modal parameters are described here:
; https://forum.rebol.info/t/1187

[
    "Basic operational test"

    (did foo: function [@x /y] [
        reduce .voidify [x y]
    ])

    ([3 ~nulled~] = foo 3)
    ([3 #] = foo @(1 + 2))
    ([@(1 + 2) ~nulled~] = foo '@(1 + 2))

    (did item: 300)

    ([304 ~nulled~] = foo item + 4)
    ([304 #] = foo @(item + 4))
    ([@(item + 4) ~nulled~] = foo '@(item + 4))

    ([300 ~nulled~] = foo item)
    ([300 #] = foo @item)
    ([@item ~nulled~] = foo '@item)

    ([[a b] ~nulled~] = foo [a b])
    ([[a b] #] = foo @[a b])
    ([@[a b] ~nulled~] = foo '@[a b])

    (did obj: make object! [field: 1020])

    ([1020 ~nulled~] = foo obj/field)
    ([1020 #] = foo @obj/field)
    ([@obj/field ~nulled~] = foo '@obj/field)
]

[
    "Basic infix operational test"

    (did bar: enfix function [@x /y] [
        reduce .voidify [x y]
    ])

    (3 bar = [3 ~nulled~])
    (@(1 + 2) bar = [3 #])

    (did item: 300)

    ((item + 4) bar = [304 ~nulled~])
    (@(item + 4) bar = [304 #])

    (item bar = [300 ~nulled~])
    (@item bar = [300 #])

    ([a b] bar = [[a b] ~nulled~])
    (@[a b] bar = [[a b] #])

    (did obj: make object! [field: 1020])

    (obj/field bar = [1020 ~nulled~])
    (@obj/field bar = [1020 #])
]

[
    "Demodalizing specialization test"

    (did foo: function [a @x /y] [
        reduce .voidify [a x y]
    ])

    ([a @x /y] = parameters of :foo)

    ([10 20 ~nulled~] = foo 10 20)
    ([10 20 #] = foo 10 @(20))

    (did fooy: :foo/y)

    ([a x] = parameters of :fooy)
    ([10 20 #] = fooy 10 20)
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
    (sensor: func [@arg [<opt> <end> any-value!] /modal] [
        reduce .try [arg modal semiquoted? 'arg]
    ] true)

    ([_ # #[false]] = sensor @(null))
    ([_ # #[true]] = sensor @())
    ([_ # #[true]] = sensor @(comment "hi"))

    ; ([_ # #[true]] = sensor @nihil)  ; !!! maybe this should work?
]
