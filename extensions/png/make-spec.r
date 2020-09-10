REBOL []
name: 'PNG
source: %png/mod-png.c
definitions: copy [
    ;
    ; Rebol already includes zlib, and LodePNG is hooked to that
    ; copy of zlib exported as part of the internal API.
    ;
    "LODEPNG_NO_COMPILE_ZLIB"

    ; LodePNG doesn't take a target buffer pointer to compress "into".
    ; Instead, you hook it by giving it an allocator.  The one used
    ; by Rebol backs the memory with a series, so that the image data
    ; may be registered with the garbage collector.
    ;
    "LODEPNG_NO_COMPILE_ALLOCATORS"

    ; With LodePNG, using C++ compilation creates a dependency on
    ; std::vector.  This is conditional on __cplusplus, but there's
    ; an #ifdef saying that even if you're compiling as C++ to not
    ; do this.  It's not an interesting debug usage of C++, however,
    ; so there's no reason to be doing it.
    ;
    "LODEPNG_NO_COMPILE_CPP"

    ; We don't want LodePNG's disk I/O routines--we use READ and write
    ;
    "LODEPNG_NO_COMPILE_DISK"

    ; There's an option for handling "ancillary chunks".  These are things
    ; like text or embedded ICC profiles:
    ; http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html
    ;
    ; Until someone requests such a feature and a way to interface with it,
    ; support for them will just make the executable bigger.  Leave these
    ; out for now.
    ;
    "LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS"
]
includes: [
    %prep/extensions/png
]
depends: [
    [
        %png/lodepng.c

        ; May 2018 update to MSVC 2017 added warnings about Spectre
        ; mitigation.  The JPG code contains a lot of code that would
        ; trigger slowdown.  It is not a priority to rewrite, given
        ; that some other vetted 3rd party JPG code should be used.
        ;
        <msc:/wd5045>  ; https://stackoverflow.com/q/50399940

        ; The LodePNG module has local scopes with declarations that
        ; alias declarations in outer scopes.  This can be confusing,
        ; so it's avoided in the core, but LodePNG is maintained by
        ; someone else with different standards.
        ;
        ;    declaration of 'identifier' hides previous
        ;    local declaration
        ;
        <msc:/wd4456>

        ; This line causes the warning "result of 32-bit shift
        ; implicitly converted to 64-bits" in MSVC 64-bit builds:
        ;
        ;     size_t palsize = 1u << mode_out->bitdepth;
        ;
        ; It could be changed to `((size_t)1) << ...` and avoid it.
        ;
        <msc:/wd4334>

        ; There is a casting away of const qualifiers, which is bad,
        ; but the PR to fix it has not been merged to LodePNG master.
        ;
        <gnu:-Wno-cast-qual>

        ; Comparison of unsigned enum value is >= 0.  A pending PR that has
        ; not been merged mentions this...update lodepng if that happens:
        ;
        ; https://github.com/lvandeve/lodepng/pull/135
        ;
        <gnu:-Wno-type-limits>
    ]
]
