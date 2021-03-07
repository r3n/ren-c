REBOL [
    File: %android-cross-compiled.r
    Inherits: %android-common.r
]


toolset: compose [
    gcc (tool-for-host/host <compiler> 'linux-x86_64)
    ld  (tool-for-host/host <linker> 'linux-x86_64)
]


cflags: reduce [
    sysroot-for-compile
    unspaced ["-D__ANDROID_API__=" android-api-level]
]


ldflags: reduce [
    sysroot-for-link
]
