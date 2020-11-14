; datatypes/file.r
(file? %myscript.r)
(not file? 1)
(file! = type of %myscript.r)
; minimum
(file? %"")
(%"" == #[file! ""])
(%"" == make file! 0)
(%"" == to file! "")
("%%2520" = mold to file! "%20")
[#1241
    (file? %"/c/Program Files (x86)")
]

[#1675 (
    files: read %./
    elide (mold files) ; once upon a time this MOLD crashed periodically
    block? files
)]
[#675 (
    files: read %../datatypes/
    elide (mold files)
    block? files
)]

[#2378 (
    some-file: %foo/baz/
    error? trap [some-file/bar/]
)]
