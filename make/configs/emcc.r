REBOL []
os-id: 0.99.1

toolset: [
    emcc
    emcc-link
]

extensions: [
    - ODBC _
]
with-ffi: no

ldflags: ["--emrun"]
