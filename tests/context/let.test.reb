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

; If a LET receives a BLOCK!, then anything that is quoted will be dequoted
; and slipped into the stream to be handled normally.
[(
    saved: _
    leftq: enfixed func ['x] [saved: x]
    let [a 'b ''(c)]: leftq
    saved = just [a b '(c)]:
)(
    leftq: enfixed func ['x] [saved: x]
    saved: let [a 'b ''(c)]
    saved = [a b '(c)]
)]

; The quoting property of LET is used to subvert a LET binding during a
; multiple-return value scenario, allowing you to mix and match variables
; which are getting new bindings with existing bindings.
(
    value: <value>
    pos: <pos>
    result: do [
        let [value 'pos]: transcode "[first item] #residue"
        reduce [value pos]
    ]
    did all [
        result = [[first item] " #residue"]
        pos = " #residue"
        value = <value>
    ]
)

; GROUP!s can either be evaluated on behalf of the LET, or if escaped they
; will be evaluated on behalf of the multi-return.  (At the moment, multi
; return does not support GROUP!s)
(
    value: <value>
    pos: <pos>
    word: 'value
    result: do [
        let [(word) 'pos]: transcode "[first item] #residue"
        reduce [value pos]
    ]
    did all [
        result = [[first item] " #residue"]
        pos = " #residue"
        value = <value>
    ]
)

; Evaluation steps need to be able to carry forward the aggregate specifier
; on their return results, otherwise the LET would be forgotten each time
; you make a step.
;
; !!! This leads to some bad properties if you try to seek around in the
; block you get back, e.g. if you run it again or try to do lookups of the
; words, you'll get out of sync stuff:
;
; https://forum.rebol.info/t/1496
(
    x: <user>
    output: '~unset~
    block: evaluate evaluate evaluate [let x: 10 output: x]
    did all [
        block = []
        output = 10
        x = <user>
    ]
)

; ADD-LET-BINDING is a conceptual step for making your own LET-like thing.
(
    maker: func [name] [
        frame: binding of 'return
        set (add-let-binding frame to word! unspaced [name 1]) <one>
        set (add-let-binding frame to word! unspaced [name 2]) <two>
    ]
    maker "demo"
    did all [
        demo1 = <one>
        demo2 = <two>
    ]
)
