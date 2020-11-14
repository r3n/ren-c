REBOL []

name: 'Library
source: %library/mod-library.c
includes: [
    %prep/extensions/library
]

depends: compose [
    (switch system-config/os-base [
        'Windows [
            [%library/library-windows.c]
        ]
    ] else [
        [%library/library-posix.c]
    ])
]
