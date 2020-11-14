REBOL [
    Title: "ODBC Open Database Connectivity Scheme"

    Name: ODBC
    Type: Module
    Options: [isolate]

    Version: 0.6.0

    Rights: [
        "Copyright (C) 2010-2011 Christian Ensel" (MIT License)
        "Copyright (C) 2017-2019 Ren-C Open Source Contributors" (Apache)
    ]

    License: {Apache 2.0}
]

; These are the native coded support routines that are needed to be built from
; C in order to interface with ODBC.  The scheme code is the usermode support
; to provide a higher level interface.
;
; open-connection: native [connection [object!] spec [text!]]
; open-statement: native [connection [object!] statement [object!]]
; insert-odbc: native [statement [object!] sql [block!]]
; copy-odbc: native [statement [object!] length [integer!]]
; close-statement: native [statement [object!]]
; close-connection: native [connection [object!]]
; update-odbc: native [connection [object!] access [logic!] commit [logic!]]


database-prototype: context [
    henv: _ ; SQLHENV handle!
    hdbc: _ ; SQLHDBC handle!
    statements:  [] ; statement objects
]

statement-prototype: context [
    database: ;
    hstmt: _ ; SQLHSTMT
    string: _
    titles: _
    columns: _
]

sys/make-scheme [
    name:  'odbc
    title: "ODBC Open Database Connectivity Scheme"

    actor: context [
        open: function [
            {Open a database port}
            port [port!]
                {WORD! spec then assume DSN, else BLOCK! DSN-less datasource}
        ][
            port/state: context [
                access: 'write
                commit: 'auto
            ]

            port/locals: make database-prototype []

            result: open-connection port/locals case [
                text? spec: select port/spec 'target [spec]
                text? spec: select port/spec 'host [unspaced ["dsn=" spec]]

                cause-error 'access 'invalid-spec port/spec
            ]

            port
        ]

        pick: function [
            port [port!]
            index
                {Index to pick from (only supports 1, for FIRST)}
        ][
            statement: make statement-prototype []

            database: statement/database: port/locals

            open-statement database statement

            port: lib/open port/spec/ref
            port/locals: statement

            append database/statements port

            port
        ]

        update: function [port [port!]] [
            if get in connection: port/locals 'hdbc [
                (update-odbc
                    connection
                    port/state/access = 'write
                    port/state/commit = 'auto
                )
                return port
            ]
        ]

        close: function [
            {Closes a statement port only or a database port w/all statements}
            return: <void>
            port [port!]
        ][
            if get try in (statement: port/locals) 'hstmt [
                remove find head statement/database/statements port
                close-statement statement
                return
            ]

            if get try in (connection: port/locals) 'hdbc [
                for-each stmt-port connection/statements [close stmt-port]
                clear connection/statements
                close-connection connection
                return
            ]
        ]

        insert: function [
            port [port!]
            sql [text! word! block!]
                {SQL statement or catalog, parameter blocks are reduced first}
        ][
            insert-odbc port/locals reduce compose [((sql))]
        ]

        copy: function [port [port!] /part [integer!]] [
            copy-odbc/part port/locals part
        ]
    ]
]


; https://forum.rebol.info/t/1234
;
odbc-execute: func [
    {Run a query in the ODBC extension using the Rebol SQL query dialect}

    statement [port!]
    query "SQL text, or block that runs SPACED with @(...) as parameters"
        [text! block!]
    /parameters "Explicit parameters (used if SQL string contains `?`)"
        [block!]
    /verbose "Show the SQL string before running it"
][
    parameters: default [copy []]

    if block? query [
        ;
        ; REDUCE first, in case code portions took @xxx as function args
        ;
        query: spaced map-each item reduce query [
            switch type of item [
                sym-word! sym-path! [
                    get item
                ]
                sym-group! [
                        reduce as group! item
                ]
            ] then value -> [
                append parameters :value
                "?"
            ] else [
                ; REDUCE may have evaluated items and made WORD!/etc.
                ; But SPACED reduces implicitly.  So map to a quoted
                ; version to avoid double-evaluations.
                ;
                quote :item
            ]
        ]
    ]

    if verbose [
        print [">> SQL:" mold query]
    ]

    insert statement compose [(query) ((parameters))]
]

sys/export [odbc-execute]
