REBOL []

name: 'Serial
source: %serial/mod-serial.c
includes: [
    %prep/extensions/serial
]

depends: compose [
    (switch system-config/os-base [
        'Windows [
            [%serial/serial-windows.c]
        ]
    ] else [
        [%serial/serial-posix.c]
    ])
]


