; functions/control/leave.r
(
    success: true
    f1: func [return: <none>] [return, success: false]
    f1
    success
)
(
    f1: func [return: <none>] [return]
    '~none~ = ^ f1
)
[#1515 ; the "result" of an arity-0 return should not be assignable
    (a: 1 reeval func [return: <none>] [a: return] :a =? 1)
]
(a: 1 reeval func [return: <none>] [set 'a return] :a =? 1)
(a: 1 reeval func [return: <none>] [set/opt 'a return] :a =? 1)
[#1509 ; the "result" of an arity-0 return should not be passable to functions
    (a: 1 reeval func [return: <none>] [a: error? return] :a =? 1)
]
[#1535
    (reeval func [return: <none>] [words of return] true)
]
(reeval func [return: <none>] [values of return] true)
[#1945
    (reeval func [return: <none>] [spec-of return] true)
]
