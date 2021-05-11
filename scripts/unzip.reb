REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Zip and Unzip Services"
    Rights: {
        Copyright 2009 Vincent Ecuyer
        Copyright 2009-2019 Ren-C Open Source Contributors
        REBOL is a trademark of REBOL Technologies

        See README.md and CREDITS.md for more information.
    }
    License: {
        Original code from %rebzip.r from www.rebol.org
        Public Domain License
    }
    Notes: {
        Only DEFLATE and STORE methods are supported.

        == archiving: zip ==

        you can zip a single file:
            zip %new-zip.zip %my-file

        a block of files:
            zip %new-zip.zip [%file-1.txt %file-2.exe]

        a block of data (binary!/text!) and files:
            zip %new-zip.zip [%my-file "my data"]

        a entire directory:
            zip/deep %new-zip.zip %my-directory/

        from a url:
            zip %new-zip.zip ftp://192.168.1.10/my-file.txt

        any combination of these:
            zip/deep %new-zip.zip  [
                %readme.txt "An example"
                ftp://192.168.1.10/my-file.txt
                %my-directory
            ]

        == unarchiving: unzip ==

        you can uncompress to a directory (created if it does not exist):
            unzip %my-new-dir %my-zip-file.zip

        or a block:
            unzip my-block %my-zip-file.zip

            my-block == [%file-1.txt #{...} %file-2.exe #{...}]
    }
]

ctx-zip: context [
    local-file-sig: #{504B0304}
    central-file-sig: #{504B0102}
    end-of-central-sig: #{504B0506}
    data-descriptor-sig: #{504B0708}

    to-ilong: (<- enbin [LE + 4])  ; Little endian 4-byte positive integer

    to-ishort: (<- enbin [LE + 2])  ; Little endian 2-byte positive integer

    to-long: (<- enbin [BE + 4])  ; Big endian 4-byte positive integer

    to-msdos-time: func [
        "Converts to a msdos time."
        value [time!] "AnyValue to convert"
    ][
        to-ishort (value/hour * 2048)
            or+ (value/minute * 32)
            or+ to integer! value/second / 2
    ]

    to-msdos-date: func [
        "Converts to a msdos date."
        value [date!]
    ][
        to-ishort 512 * (max 0 value/year - 1980)
            or+ (value/month * 32) or+ value/day
    ]

    get-msdos-time: func [
        "Converts from a msdos time."
        value [binary! port!]
    ][
        value: debin [LE + 2] value
        to time! reduce [
            63488 and+ value / 2048
            2016 and+ value / 32
            31 and+ value * 2
        ]
    ]

    get-msdos-date: func [
        "Converts from a msdos date."
        value [binary! port!]
    ][
        value: debin [LE + 2] value
        to date! reduce [
            65024 and+ value / 512 + 1980
            480 and+ value / 32
            31 and+ value
        ]
    ]

    zip-entry: func [
        {Compresses a file}

        return: "local header"
            [binary!]
        central-dir-entry: "Central Directory entry"
            [binary!]

        name "Name of file"
            [file!]
        date "Modification date of file"
            [date!]
        data "Data to compress"
            [binary!]
        offset "Offset where the compressed entry will be stored in the file"
            [integer!]
    ][
        ; info on data before compression
        let crc: checksum-core 'crc32 data

        let uncompressed-size: to-ilong length of data

        let compressed-data: deflate data

        let method
        if (length of compressed-data) < (length of data) [
            method: 'deflate
        ] else [
            method: 'store  ; deflating didn't help

            clear compressed-data  ; !!! doesn't reclaim memory (...FREE ?)
            compressed-data: data
        ]

        let compressed-size: to-ilong length of compressed-data

        ; central-dir file entry.  note that the file attributes are
        ; interpreted based on the OS of origin--can't say Amiga :-(
        ;
        assert [central-dir-entry]  ; currently expects you request it
        set central-dir-entry join-all [
            central-file-sig
            #{1E}  ; version of zip spec this encoder speaks (#{1E}=3.0)
            #{03}  ; OS of origin: 0=DOS, 3=Unix, 7=Mac, 1=Amiga...
            #{0A00}  ; minimum spec version for decoder (#{0A00}=1.0)
            #{0000}  ; flags
            switch method ['store [#{0000}] 'deflate [#{0800}] fail]
            to-msdos-time date/time
            to-msdos-date date/date
            crc  ; crc-32
            compressed-size
            uncompressed-size
            to-ishort length of name  ; filename length
            #{0000}  ; extrafield length
            #{0000}  ; filecomment length
            #{0000}  ; disknumber start
            #{0100}  ; internal attributes (Mac puts #{0100} vs. #{0000})
            #{0000A481}  ; external attributes, this is `-rw-r--r--`
            to-ilong offset  ; header offset
            name  ; filename
            comment <extrafield>  ; not used
            comment <filecomment>  ; not used
        ]

        ; local file entry
        ;
        return join-all [
            local-file-sig
            #{0A00}  ; version (both Mac OS Zip and Linux Zip put #{0A00})
            #{0000}  ; flags
            switch method ['store [#{0000}] 'deflate [#{0800}] fail]
            to-msdos-time date/time
            to-msdos-date date/date
            crc  ; crc-32
            compressed-size
            uncompressed-size
            to-ishort length of name  ; filename length
            #{0000}  ; extrafield length
            name  ; filename
            comment <extrafield>  ; not used
            compressed-data
        ]
    ]

    to-path-file: func [
        {Converts url! to file! and removes heading "/"}
        value [file! url!] "AnyValue to convert"
    ][
        if file? value [
            if #"/" = first value [value: copy next value]
            return value
        ]
        value: decode-url value
        join %"" [
            value/host "/"
            any [value/path ""]
            any [value/target ""]
        ]
    ]

    zip: func [
        {Build zip archive from a file or dialected data specification block}

        return: "Number of entries in archive"
            [integer!]
        where "Where to build the archive (allows series in-memory)"
            [file! url! binary! text!]
        source "Files to archive (only STORE and DEFLATE supported)"
            [file! url! block!]
        /deep "Includes files in subdirectories"
        /verbose "Lists files while compressing"
        /only "Include the root source directory"
    ][
        let info: if not verbose [:elide] else [:print]

        if match [file! url!] where [
            where: open/write where
        ]

        let out: func [value] [append where value]

        let offset: 0
        let num-entries: 0
        let central-directory: copy #{}

        let root
        all [not only, file? source, dir? source] then [
            root: source
            source: read source
        ] else [
            root: %./
        ]

        source: to block! source
        iterate source [
            let name: source/1
            let root+name: if find "\/" name/1 [
                info ["Warning: absolute path" name]
                name
            ] else [%% (root)/(name)]

            let no-modes: did any [url? root+name, dir? root+name]

            all [deep, dir? name] then [
                name: dirize name
                let files: ensure block! read root+name
                for-each file files [
                    append source %% (name)/(file)
                ]
                continue
            ]

            num-entries: num-entries + 1

            ; !!! Zip builds on NOW, however this function comes from an
            ; extension.  Yet the Unzip which lives in this same module is
            ; needed to unpack encapping, and may be required before the
            ; Time extension has been loaded.  A simple reference to NOW
            ; won't find the function that didn't exist at module load time,
            ; so use LIB/NOW to force lookup through LIB.
            ;
            let date: lib/now  ; !!! Each file has slightly later date?

            let data: if match [binary! text!] :source/2 [  ; next is data
                first (source: next source)
            ] else [  ; otherwise data comes from reading the location itself
                if dir? name [
                    copy #{}
                ] else [
                    if not no-modes [
                        date: modified? root+name
                    ]
                    read root+name
                ]
            ]

            if not binary? data [data: to binary! data]

            name: to-path-file name
            info [name]

            let [file-entry dir-entry]: zip-entry name date data offset

            append central-directory dir-entry

            append where file-entry
            offset: me + length of file-entry
        ]

        append where join-all [
            central-directory
            end-of-central-sig
            #{0000}  ; disk num
            #{0000}  ; disk central dir
            to-ishort num-entries  ; num entries disk
            to-ishort num-entries  ; num entries
            to-ilong length of central-directory
            to-ilong offset  ; offset of the central directory
            #{0000}  ; zip file comment length
            comment <zipfilecomment>  ; not used
        ]
        if port? where [close where]
        return num-entries
    ]

    unzip: function [
        {Decompresses a zip archive to a directory or a block}

        return: "If `where` was a block, then position after archive insertion"
            [void! block!]
        where "Where to decompress it"
            [file! block!]
        source "Archive to decompress (only STORE and DEFLATE supported)"
            [file! url! binary!]
        /verbose "Lists files while decompressing (default)"
        /quiet "Don't lists files while decompressing"
    ][
        num-errors: 0
        info: all [quiet, not verbose] then [:elide] else [:print]
        if not block? where [
            where: my dirize
            if not exists? where [make-dir/deep where]
        ]
        if match [file! url!] source [
            source: read source
        ]

        ; !!! LET is not implemented in UPARSE yet, which means creating
        ; utility rules like this have trouble with name overlap in the
        ; enclosing routine.  To be addressed soon.
        ;
        uint16-rule: [tmpbin: across 2 skip, (debin [LE + 2] tmpbin)]
        uint32-rule: [tmpbin: across 4 skip, (debin [LE + 4] tmpbin)]
        msdos-date-rule: [tmpbin: across 2 skip, (get-msdos-date tmpbin)]
        msdos-time-rule: [tmpbin: across 2 skip, (get-msdos-time tmpbin)]

        ; NOTE: The original rebzip.r did decompression based on the local file
        ; header records in the zip file.  But due to streaming compression
        ; these can be incomplete and have zeros for the data sizes.  The only
        ; reliable source of sizes comes from the central file directory at
        ; the end of the archive.  That might seem redundant to those not aware
        ; of the streaming ZIP debacle, because a non-streaming zip can be
        ; decompressed without it...but streaming files definitely exist!
        ;
        ; (See %tests/fixtures/test.docx for an example of a file that the
        ; original rebzip could not unzip.)

        ; Finding the central directory is done empirically by scanning from
        ; the end of file, looking for the end-of-central-sig.
        ;
        if not central-end-pos: find-reverse (tail source) end-of-central-sig [
            fail "Could not find end of central directory signature"
        ]
        uparse central-end-pos [
            end-of-central-sig  ; by definition (pos matched this)

            2 skip  ; disk_nbr
            2 skip  ; cd_start_disk
            2 skip  ; disk_cd_entries
            num-central-entries: uint16-rule
            total-central-directory-size: uint32-rule
            central-directory-offset: uint32-rule
            archive-comment-len: uint16-rule

            ; We don't care about the archive comment (though we could extract
            ; it optionally, here).  But we can check that the length would
            ; reach the end.  This could be thrown off if the comment itself
            ; contains the end-of-central-sig, which is not formally prohibited
            ; by the spec (though some suggest it should be).
            ;
            archive-comment-len skip end
        ] else [
            fail "Malformed end of central directory record"
        ]

        ; This rule extracts the information out of the central directory and
        ; into local variables.
        ;
        ; !!! Review if this would be better done as a GATHER into an object,
        ; as SET-WORD! gathering (e.g. FUNCT-ION) is falling from favor.
        ;
        central-directory-entry-rule: [
            [central-file-sig | (fail "CENTRAL-FILE-SIG mismatch")]

            version-created-by: across 2 skip  ; version that made this file
            version-needed: across 2 skip  ; version needed to extract
            flags: across 2 skip

            method-number: uint16-rule
            time: msdos-time-rule
            date: msdos-date-rule
            crc: across 4 skip  ; crc32 little endian, maybe 0 in local header
            compressed-size: uint32-rule  ; maybe 0 in local header
            uncompressed-size: uint32-rule  ; maybe 0 in local header

            name-length: uint16-rule
            extra-field-length: uint16-rule
            file-comment-length: uint16-rule  ; not in local header
            disk-number-start: uint16-rule  ; not in local header
            internal-attributes: across 2 skip  ; not in local header
            external-attributes: across 4 skip  ; not in local header
            local-header-offset: uint32-rule  ; (for finding local header)
            name: [temp: across name-length skip, (to-file temp)]

            extra-field-length skip  ; !!! Expose "extra" field?
            file-comment-length skip  ; !!! Expose file comment?
        ]

        ; When it was realized that the old rebzip.r method of relying on the
        ; local directory entries would not work, code was added to check for
        ; coherence between the central directory and the local entries.  This
        ; may be overkill, but it's a sanity check that may help security.
        ;
        ; However, consider making these checks downgradable to warnings.
        ;
        check-local-directory-entry-rule: [
            [local-file-sig | (fail "LOCAL-FILE-SIG mismatch")]

            x: across 2 skip, (assert [x = version-needed])
            x: across 2 skip, (assert [x = flags])
            x: uint16-rule, (assert [x = method-number])
            x: msdos-time-rule, (assert [x = time])
            x: msdos-date-rule, (assert [x = date])

            [
                :(not zero? flags/1 and+ 8)  ; "bit 3" -> has data descriptor
                ;
                ; "If this bit is set, the fields crc-32, compressed size, and
                ; uncompressed size are set to zero in the local header.  The
                ; correct values are put in the data descriptor immediately
                ; following the compressed data."
                ;
                ; Note: Since deflate is self-terminating, you could streaming
                ; unzip the data and then verify its size.  Most decompressors
                ; don't do this, they use the central directory instead.  So
                ; we go with that approach as well, given that there are file
                ; attributes there not available in the local header anyway.
                ;
                x: across 4 skip, (assert [x = #{00000000}])  ; crc
                x: uint32-rule, (assert [x = 0])  ; compressed size
                x: uint32-rule, (assert [x = 0])  ; uncompressed size
            |
                x: across 4 skip, (assert [x = crc])
                x: uint32-rule, (assert [x = compressed-size])
                x: uint32-rule, (assert [x = uncompressed-size])
            ]

            x: uint16-rule, (assert [x = name-length])
            x: uint16-rule, (assert [x = extra-field-length])
            x: across name-length skip, (assert [(to-file x) = name])

            extra-field-length skip
        ]

        ; While this is by no means broken up perfectly into subrules, it is
        ; clearer than it was.
        ;
        uparse (skip source central-directory-offset) [
            num-central-entries [  ; we expect to repeat this loop num times
                ;
                ; Process one central directory entry, extracting its fields
                ; into local variables for this function.
                ;
                central-directory-entry-rule
                central-file-end: here

                (info [name])

                ; Jump to the local file header location to check coherence
                ; (it's also where the compressed data actually is stored).
                ; We'll seek back to CENTRAL-FILE-END to process the next
                ; entry after the decompression.
                ;
                seek (skip source local-header-offset)
                check-local-directory-entry-rule

                ; !!! Note: the date and time information are currently not
                ; used by the extraction.  But this code was blending them
                ; into a "datetime".  Best to do that after the validation
                ; against the information in the local directory entry.
                (
                    date/time: time
                    date: date - lib/now/zone  ; see notes RE: LIB/NOW above
                )

                ; !!! TBD: Improve handling of flags.
                ;
                (if not zero? flags/1 and+ 1 [
                    fail "Encryption not supported by unzip.reb (yet)"
                ])

                ; We're now right past the local directory entry, where the
                ; compressed data is stored.
                ;
                data: here
                (
                    uncompressed-data: catch [

                        ; STORE(0) and DEFLATE(8) are the only widespread
                        ; methods used for .ZIP compression in the wild today

                        if method-number = 0 [  ; STORE
                            throw copy/part data compressed-size
                        ]

                        if method-number <> 8 [  ; DEFLATE
                            info ["-> failed [method" method-number "]"]
                            throw blank
                        ]
                        data: copy/part data compressed-size
                        trap [
                            data: inflate/max data uncompressed-size
                        ] then [
                            info "-> failed [deflate]"
                            throw blank
                        ]

                        if uncompressed-size != length of data [
                            info "-> failed [wrong output size]"
                            throw blank
                        ]

                        check: checksum-core 'crc32 data
                        if crc != check [
                            info "-> failed [bad crc32]"
                            print [
                                "expected crc:" crc LF
                                "actual crc:" check
                            ]
                            throw data
                        ]

                        throw data
                    ]

                    either uncompressed-data [
                        info ["-> ok [deflate]"]
                    ][
                        num-errors: me + 1
                    ]

                    either any-array? where [
                        where: insert where name
                        where: insert where all [
                            #"/" = last name
                            empty? uncompressed-data
                        ] then [blank] else [uncompressed-data]
                    ][
                        ; make directory and/or write file
                        either #"/" = last name [
                            if not exists? %% (where)/(name) [
                                make-dir/deep %%(where)/(name)
                            ]
                        ][
                            set [path: file:] split-path name
                            if not exists? %% (where)/(path) [
                                make-dir/deep %% (where)/(path)
                            ]
                            if uncompressed-data [
                                write %% (where)/(name) uncompressed-data

                                ; !!! R3-Alpha didn't support SET-MODES
                                comment [
                                    set-modes %% (where)/(name) [
                                        modification-date: date
                                    ]
                                ]
                            ]
                        ]
                    ]
                )

                ; Jump back to the central directory point where we left off...
                ;
                seek (central-file-end)
            ]

            [
                ahead end-of-central-sig  ; after entries should be end sig
                | (fail "Bad central directory termination")  ; else fail
            ]

            ; We shouldn't just be at *an* end-of-central signature, we should
            ; be at the end record we started the search from.
            ;
            pos: here, (assert [pos = central-end-pos])

            to end  ; skip to end so parse succeeds (could RETURN TRUE)
        ] else [
            fail "Malformed Zip Archive"
        ]

        if block? where [return where]
        return
    ]
]

append lib compose [
    zip: (:ctx-zip/zip)
    unzip: (:ctx-zip/unzip)
]
