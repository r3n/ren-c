; functions/convert/load.r
[#20
    ([1] = load "1")
]
[#22 ; a
    ((quote the :a) = load-value "':a")
]
[#22 ; b
    (error? trap [load-value "':a:"])
]
[#858 (
    a: [ < ]
    a = load-value mold a
)]
(error? trap [load "1xyz#"])

; LOAD/NEXT removed, see #1703
;
(error? trap [load/next "1"])


[#1122 (
    any [
        error? trap [load "9999999999999999999"]
        greater? load-value "9999999999999999999" load-value "9223372036854775807"
    ]
)]
; R2 bug
(
     x: 1
     x: load/header "" 'header
     did all [
        x = []
        null? header
     ]
)

[#1421 (
    did all [
        error? trap [load "[a<]"]
        error? trap [load "[a>]"]
        error? trap [load "[a+<]"]
        error? trap [load "[1<]"]
        error? trap [load "[+a<]"]
    ]
)]
