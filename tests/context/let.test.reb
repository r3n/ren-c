# %let.test.reb
#
# LET is a construct which dynamically creates a variable binding and injects
# it into the stream following it.  This makes it a nicer syntax than USE as
# it does not require introducing a new indentation/block level.  However,
# like USE it does have the effect of doing an allocation each time, which
# means it can create a lot of load for the GC, especially in loops:
#
#     count-up x 1000000 [
#         let y: x + 1  ; allocates y each time, hence a million allocs
#


; Because things are currently bound in the user context by default, it can
; be hard to test whether it is adding new bindings or not.
[
    (
       x: <in-user-context>
       did all [
           1020 = do compose [let (unbind 'x:) 20, 1000 + (unbind 'x)]
           x = <in-user-context>
       ]
    )
    (
       x: <in-user-context>
       did all [
           1020 = do compose [let x: 20, 1000 + x]
           x = <in-user-context>
       ]
    )
]

; LET X: 10 form declares but doesn't initialize, and returns
[(
        x: <in-user-context>
        [<in-user-context> 10 10] = reduce [x, let x: 10, get 'x]
)
(
        x: <in-user-context>
        [<in-user-context> x 10 10] = reduce [x, let x, x: 10, get 'x]
)]
