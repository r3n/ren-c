; %frame.test.reb
;
; The FRAME! type is foundational to the mechanics of Ren-C vs. historical
; Rebol.  While its underlying storage is similar to an OBJECT!, it has a
; more complex mechanic based on being able to be seen through the lens of
; multiple different "views" based on which phase of a function composition
; it has been captured for.

[
    (
        foo: func [public <local> private] [
            private: 304
            return binding of 'public  ; frame as seen from inside
        ]

        f-outside: make frame! :foo  ; frame as seen from outside
        f-outside/public: 1020

        f-inside: do copy f-outside
        true
    )

    (f-outside/public = 1020)  ; public values visible externally
    (
        e: trap [f-outside/private]
        'bad-path-pick = e/id  ; private not visible in external view
    )

    (f-inside/public = 1020)  ; public values still visible internally
    (f-inside/private = 304)  ; returned internal view exposes private fields

    ; === ADAPT ===
    ;
    ; Inside adaptation, we should not be able to see or manipulate any
    ; locals of the underlying function.
    ;
    ; !!! Taking advantage of the space available in locals could be
    ; interesting if possible to rename them, and then reset them to
    ; undefined while typechecking for lower level phases.  Think about it.
    (
        f-inside-prelude: ~
        private: <not-in-prelude>
        adapted-foo: adapt :foo [
            f-inside-prelude: binding of 'public
            assert [private = <not-in-prelude>]  ; should not be bound
        ]

        f-outside-adapt: make frame! :adapted-foo
        f-outside-adapt/public: 1020

        f-inside-foo: do copy f-outside-adapt

        true
    )

    (f-outside-adapt/public = 1020)
    (
        e: trap [f-outside-adapt/private]
        'bad-path-pick = e/id
    )

    (f-inside-prelude/public = 1020)
    (
        e: trap [f-inside-prelude/private]
        'bad-path-pick = e/id
    )

    (f-inside-foo/public = 1020)
    (f-inside/private = 304)

    ; === AUGMENT ===
    ;
    ; An augmentation makes a new parameter that is not visible to layers
    ; below it.  Notably, due to information-hiding, this parameter may have
    ; the same name as a hidden implementation detail of the inner portions
    ; of the composition.
    (
        f-inside-augment: ~
        private: <not-in-prelude>

        augmented-foo: adapt (augment :adapted-foo [
            additional [integer!]
            /private [tag!]  ; reusing name, for different variable!
        ]) [
            private: <reused>
            f-inside-augment: binding of 'private
            print mold f-inside-augment
        ]

        assert [private = <not-in-prelude>]  ; should be untouched

        f-outside-augment: make frame! :augmented-foo
        f-outside-augment/public: 1020
        f-outside-augment/additional: 1020304

        f-inside-foo: do copy f-outside-augment

        true
    )

    (f-outside-augment/public = 1020)
    (f-outside-augment/additional = 1020304)
    (~undefined~ = get/any 'f-outside-augment/private)  ; we didn't assign it

    (f-inside-augment/public = 1020)
    (f-inside-augment/additional = 1020304)
    (f-inside-augment/private = <reused>)  ; not 304!

    (f-inside-foo/public = 1020)
    (f-inside-foo/private = 304)  ; not reused!
    (
        e: trap [f-inside-foo/additional]
        'bad-path-pick = e/id
    )
]


; Shorter AUGMENT scenario
[
    (
        foo: func [public <local> private] [
            private: 304
            return binding of 'public  ; return FRAME! with the internal view
        ]

        f-prelude: null

        bar: adapt augment :foo [/private [tag!]] [
            f-prelude: binding of 'private
        ]

        f-outside: make frame! :bar
        f-outside/public: 1020
        f-outside/private: <different!>

        f-inside: do f-outside

        true
    )

    (f-prelude/public = 1020)
    (f-prelude/private = <different!>)

    (f-inside/public = 1020)
    (f-inside/private = 304)
]
