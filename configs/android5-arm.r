REBOL [
    File: %android5-arm.r
]

os-id: 0.13.2

tool-prefix: to-file try get-env "ANDROID_NDK"

gcc-path: make-file [
    (tool-prefix) / toolchains /
        arm-linux-androideabi-4.9 / prebuilt / linux-x86_64 / bin /
        arm-linux-androideabi-gcc
]

toolset: compose [
    gcc (gcc-path)
    ld  (gcc-path)
]

ldflags: cflags: reduce [
    unspaced ["--sysroot=" make-file [
        (tool-prefix) / platforms / android-19 / arch-arm
    ]]
]
