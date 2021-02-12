; datatypes/module.r

({REBOL [Title: "Test"]} = find-script {;234^/REBOL [Title: "Test"]})

(module? module [] [])
(not module? 1)
(module! = type of module [] [])

(
    a-module: module [
    ] [
        ; 'var will be in the module
        var: 1
    ]
    var: 2
    1 == a-module/var
)

; import test
(
    a-module: module [
        exports: [var]
    ] [
        var: 2
    ]
    import a-module
    2 == var
)

; import test
(
    var: 1
    a-module: module [
        exports: [var]
    ] [
        var: 2
    ]
    import a-module
    1 == var
)


([] = load " ")
([1] = load "1")
([[1]] = load "[1]")
([1 2 3] = load "1 2 3")
([1 2 3] = load/type "1 2 3" null)
([1 2 3] = load "rebol [] 1 2 3")
(
    d: load/header "rebol [] 1 2 3" 'header
    all [
        object? header
        [1 2 3] = d
    ]
)

; This was a test from the %sys-load.r which trips up the loading mechanic
; (at time of writing).  LOAD thinks that the entirety of the script is the
; "rebol [] 1 2 3", and skips the equality comparison etc. so it gets
; loaded as [1 2 3], which then evaluates to 3.  The test framework then
; considers that "not a logic".
;
; ([1 2 3] = load "rebol [] 1 2 3")

; File variations:
(equal? read %./ load %./)
(
    write %test.txt s: "test of text"
    s = load %test.txt
)
(
    save %test1.r 1
    1 = load-value %test1.r
)
(
    save %test2.r [1 2]
    [1 2] = load %test2.r
)
(
    save/header %test.r [1 2 3] [title: "Test"]
    [1 2 3] = load %test.r
)
(
    save/header %test-checksum.r [1 2 3] [checksum: true]
    [1 2 3] = load %test-checksum.r
)
(
    save/header %test-checksum.r [1 2 3] [checksum: true compress: true]
    [1 2 3] = load %test-checksum.r
)
(
    save/header %test-checksum.r [1 2 3] [checksum: script compress: true]
    [1 2 3] = load %test-checksum.r
)
