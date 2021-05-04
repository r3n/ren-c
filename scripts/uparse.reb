Rebol [
    Title: {UPARSE: Usermode Implementation of PARSE in Ren-C}
    License: {LGPL 3.0}

    Description: {
        Rebol's PARSE is a tool for performing RegEx-style tasks using an
        English-like dialect.  It permits breaking down complex expressions
        into named subrules, and has a very freeform model for escaping into
        and back out of imperative code:

        http://www.rebol.com/docs/core23/rebolcore-15.html
        http://www.codeconscious.com/rebol/parse-tutorial.html
        https://www.red-lang.org/2013/11/041-introducing-parse.html

        The implementation of PARSE engines has traditionally been as
        optimized systems-level code (e.g. C or Red/System), built into the
        interpreter.  This does not offer flexibility to make any minor or
        major changes to the dialect that a user might imagine.

        This script attempts to make PARSE more "hackable", by factoring the
        implementation out so that each keyword or data behavior is handled
        by an individual `parser` function.  The parameters to this function
        are stylized so that the parse engine can compose these smaller
        parsers together as part of larger processing operations.  This
        approach is typically called "Parser Combinators":

        https://en.wikipedia.org/wiki/Parser_combinator

        While this overlaps with typical parser combinators, Rebol's design
        affords it a unique spin.  By building the backbone of the dialect as
        a BLOCK!, there's no SEQUENCE or ALTERNATIVE combinators.  Instead,
        blocks make sequencing implicit just by steps being ordered after one
        another.  The alternates are managed by means of `|` markers, which
        are detected by the implementation of the block combinator--and `|` is
        not a combinator in its own right.  With novel operators and convenient
        ways of switching into imperative processing, it gets a unique and
        literate feel with a relatively clean appearance.

        By making the combinator list passed to UPARSE as a MAP!, is possible
        to easily create overrides or variations of the dialect.  (For
        instance, a version that is compatible with older Rebols.)  But the
        goal is to facilitate even more ambitious features.
    }

    Notes: {
        * This implementation will be *extremely* slow for the foreseeable
          future.  But since it is built on usermode facilities, any
          optimizations that are applied to it will bring systemic benefits.
          Ultimately the goal is to merge this architecture in with the
          "messier" C code...hopefully preserving enough of the hackability
          while leveraging low-level optimizations where possible.
    }
]

; All combinators receive the INPUT to be processed.  They are also given an
; object representing the STATE of the parse (currently that is just the
; FRAME! of the main UPARSE call which triggered the operation, so any of the
; locals or parameters to that can be accessed, e.g. the list of /COMBINATORS)
;
; The goal of a combinator is to decide whether to match (by returning a
; new position in the series) or fail to match (by returning a NULL)
;
; Additional parameters to a combinator are fulfilled by the parse engine by
; looking at the ensuing rules in the rule block.
;
; One of the parameter types that can be given to these functions are another
; parser, to combine them (hence "parser combinator").  So you can take a
; combinator like OPT and parameterize it with SOME which is parameterized
; with "A", to get the parser `opt some "a"`.
;
; But if a parameter to a combinator is marked as quoted, then that will take
; a value from the callsite literally.
;
; A special COMBINATOR generator is used.  This saves on repetition of
; parameters and also lets the engine get its hooks into the execution of
; parsers...for instance to diagnose the furthest point the parsing reached.

combinator: func [
    {Make a stylized ACTION! that fulfills the interface of a combinator}

    spec [block!]
    body [block!]

    <static> wrapper (
        func [
            {Enclosing function for hooking all combinators}
            f [frame!]
        ][
            ; This hook lets us run code before and after each execution of
            ; the combinator.  That offers lots of potential, but for now
            ; we just use it to notice the furthest parse point reached.
            ;
            let state: f.state
            let remainder: f.remainder
            do f else [return null]
            elide all [
                state.furthest
                (index? remainder: get remainder) > (index? get state.furthest)
                set state.furthest remainder
            ]
        ]
    )
][
    let action: func compose [
        ; Get the text description if given
        ((if text? spec.1 [spec.1, elide spec: my next]))

        ; Get the RETURN: definition if there is one
        ;
        ((if set-word? spec.1 [
            assert [spec.1 = 'return:]
            assert [text? spec.2]
            assert [block? spec.3]

            ; !!! There is a concept being messed with right now based on the
            ; idea that there is a "core" form of DO which encompasses macros
            ; and invisibles.  Combinators are simulating this protocol as an
            ; early test, so what they actually deal in are NULL, QUOTED! and
            ; empty BLOCK! for now:
            ;
            ; https://forum.rebol.info/t/1590
            ;
            reduce [spec.1 spec.2 [<opt> quoted! block!]]
            elide spec: my skip 3
        ]))

        remainder: [<opt> any-series!]

        state [frame!]
        input [any-series!]

        ; Whatever arguments the combinator takes, if any
        ;
        ((spec))
    ] body

    ; Enclosing with the wrapper permits us to inject behavior before and
    ; after each combinator is executed.
    ;
    enclose :action :wrapper
]


; !!! We use a MAP! here instead of an OBJECT! because if it were MAKE OBJECT!
; then the parse keywords would override the Rebol functions (so you couldn't
; use ANY inside the implementation of a combinator, because there's a
; combinator named ANY).  This is part of the general issues with binding that
; need to have answers.
;
default-combinators: make map! reduce [

    === BASIC KEYWORDS ===

    'opt combinator [
        {If the parser given as a parameter fails, return input undisturbed}
        return: "PARSER's result if it succeeds, otherwise NULL"
            [<opt> any-value!]
        parser [action!]
        <local> result
    ][
        if [result (remainder)]: parser input [
            return result  ; on parser's success, return what it returned
        ]
        set remainder input  ; on parser's failure, make OPT remainder input
        return quote null  ; OPT succeeds on parser failure, "null-2" result
    ]

    'end combinator [
        {Only match if the input is at the end}
        return: "End position of the parse input"
            [<opt> any-series!]
    ][
        if tail? input [
            set remainder input
            return quote input  ; core return protocol
        ]
        set remainder null
        return null
    ]

    'not combinator [
        {Fail if the parser rule given succeeds, else continue}
        return: "~not~ if the rule fails, NULL if it succeeds"
            [<opt> void!]
        parser [action!]
    ][
        if [# (remainder)]: parser input [  ; don't care about result
            return null
        ]
        set remainder input  ; parser failed, so NOT reports success
        return quote '~not~  ; clearer than returning NULL
    ]

    'ahead combinator [
        {Leave the parse position at the same location, but fail if no match}
        return: "parser result if success, NULL if failure"
            [<opt> any-value!]
        parser [action!]
        <local> result
    ][
        if [result #]: parser input [  ; don't care about remainder
            return result
        ]
        return null
    ]

    'further combinator [
        {Pass through the result only if the input was advanced by the rule}
        return: "parser result if it succeeded and advanced input, else NULL"
            [<opt> any-value!]
        parser [action!]
        <local> result pos
    ][
        if not [result pos]: parser input [
            return null  ; the parse rule did not match
        ]
        if (index? pos) <= (index? input) [
            return null  ; the rule matched, but did not advance the input
        ]
        if result = [] [  ; don't allow all-invisible
            fail "Rule passed to FURTHER must synthesize a product"
        ]
        set remainder pos
        return result
    ]

    === LOOPING CONSTRUCT KEYWORDS ===

    ; ANY and WHILE were slight variations on a theme, with `any [...]` being
    ; the same as `while [not end, ...]`.  Because historical Redbol PARSE-ANY
    ; contrasts semantically with with DO-ANY (pick from alternates), we
    ; choose to make the primitive operation WHILE.
    ;
    ; https://forum.rebol.info/t/any-vs-while-and-not-end/1572
    ; https://forum.rebol.info/t/any-vs-many-in-parse-eof-tag-combinators/1540/
    ;
    ; For the moment, ANY is assigned as an alias for WHILE, but with no
    ; difference in shade of meaning.

    'while combinator [
        {Any number of matches (including 0)}
        return: "Result of last successful match"
            [<opt> any-value!]
        parser [action!]
        <local> last-result result pos
    ][
        last-result: just '  ; "core return protocol", null is not failing
        cycle [
            if not [result pos]: parser input [
                set remainder input  ; overall WHILE never fails (but REJECT?)
                return last-result
            ]
            last-result: result
            input: pos
        ]
    ]

    'some combinator [
        {Must run at least one match}
        return: "Result of last successful match"
            [<opt> any-value!]
        parser [action!]
        <local> last-result result pos
    ][
        if not [last-result input]: parser input [
            return null
        ]
        cycle [  ; if first try succeeds, flip to same code as WHILE
            if not [result pos]: parser input [
                set remainder input
                return last-result
            ]
            last-result: result
            input: pos
        ]
    ]

    'tally combinator [
        {Iterate a rule and count the number of times it matches}
        return: "Number of matches (can be 0)"
            [<opt> integer!]
        parser [action!]
        <local> count pos
    ][
        count: 0
        cycle [
            ; !!! We discard the result, but should it be available, e.g.
            ; via a multi-return?  Can PARSE rules have multi-returns?  If
            ; so, then advanced would likely have to be done another way.  :-/
            ;
            if not [# pos]: parser input [
                set remainder input
                return quote count  ; "core" return protocol
            ]
            count: count + 1
            input: pos
        ]
    ]

    === MUTATING KEYWORDS ===

    ; Topaz moved away from the idea that PARSE was used for doing mutations
    ; entirely.  It does complicate the implementation to be changing positions
    ; out from under things...so it should be considered carefully.
    ;
    ; UPARSE continues with the experiment, but does things a bit differently.
    ; Here CHANGE is designed to be used with value-bearing rules, and the
    ; value-bearing rule is run against the same position as the start of
    ; the input.
    ;
    ; !!! Review what happens if the input rule can modify, too.

    'change combinator [
        {Substitute a match with new data}
        return: "!!! TBD !!!"
            [<opt> void!]
        parser [action!]
        replacer [action!]  ; !!! How to say result is used here?
        <local> replacement
    ][
        if not [# (remainder)]: parser input [  ; first look for end position
            return null
        ]

        if not [replacement #]: replacer input [  ; then get replacement
            return null
        ]

        if replacement = [] [
            fail "Cannot CHANGE to invisible replacement"
        ]
        assert [quoted? replacement]

        ; CHANGE returns tail, use as new remainder
        ;
        set remainder change/part input (unquote replacement) (get remainder)
        return quote '~changed~
    ]

    'remove combinator [
        {Remove data that matches a parse rule}
        return: "!!!TBD"
            [<opt> void!]
        parser [action!]
    ][
        if not [# (remainder)]: parser input [  ; first look for end position
            return null
        ]

        set remainder remove/part input get remainder
        return quote '~removed~
    ]

    'insert combinator [
        {Insert literal data into the input series}
        return: "!!! TBD"
            [<opt> void!]
        parser [action!]
        <local> insertion
    ][
        if not [insertion #]: parser input [  ; remainder irrelevant
            return null
        ]

        if insertion = [] [
            fail "Cannot INSERT to invisible insertion"
        ]
        assert [quoted? insertion]

        set remainder insert input (unquote insertion)
        return quote '~inserted~
    ]

    === SEEKING KEYWORDS ===

    'to combinator [
        {Match up TO a certain rule (result position before succeeding rule)}
        return: "The rule's product"
            [<opt> any-value!]
        parser [action!]
        <local> result
    ][
        cycle [
            if [result #]: parser input [
                set remainder input  ; TO means do not include match range
                return result
            ]
            if tail? input [  ; could be `to end`, so check tail *after*
                return null
            ]
            input: next input
        ]
    ]

    'thru combinator [
        {Match up THRU a certain rule (result position after succeeding rule)}
        return: "The rule's product"
            [<opt> any-value!]
        parser [action!]
        <local> result pos
    ][
        cycle [
            if [result pos]: parser input [
                set remainder pos
                return result
            ]
            if tail? input [  ; could be `thru end`, check TAIL? *after*
                return null
            ]
            input: next input
        ]
    ]

    'here combinator [
        {Get the current parse input position, without advancing input}
        return: "parse position"
            [any-series!]
    ][
        set remainder input
        return quote input  ; "core" return protocol
    ]

    'seek combinator [
        return: "seeked position"
            [any-series!]
        parser [action!]
        <local> where
    ][
        if not [where (remainder)]: parser input [
            return null
        ]
        if where = [] [
            fail "Cannot SEEK to invisible parse rule result"
        ]
        where: my unquote
        case [
            integer? where [
                set remainder at head input where
            ]
            any-series? :where [
                if not same? head input head where [
                    fail "Series SEEK in UPARSE must be in the same series"
                ]
                set remainder where
            ]
            fail "SEEK requires INTEGER! or series position"
        ]
        return quote get remainder  ; "core" return protocol
    ]

    'between combinator [
        return: "Copy of content between the left and right parsers"
            [<opt> any-series!]
        parser-left [action!]
        parser-right [action!]
        <local> start
    ][
        if not [# start]: parser-left input [
            return null
        ]

        let limit: start
        cycle [
            if [# (remainder)]: parser-right limit [  ; found it
                return quote copy/part start limit  ; "core" return protocol
            ]
            if tail? limit [  ; remainder is null
                return null
            ]
            limit: next limit
        ]
        ; ~unreachable~
    ]

    === VALUE-BEARING KEYWORDS ===

    ; Historically SKIP was used to match one item of input.  But the word
    ; wasn't a great fit, as it could be used with assignment to actually
    ; capture the value...which is the opposite of skipping.  You might call
    ; it ACCEPT or MATCH or something more in that vein, but every choice has
    ; some downsides.
    ;
    ; This is under scrutiny.  `*` was chosen in Topaz, but that means "match
    ; any number of" in RegEx and wildcarding.  So it's a poor choice.  Other
    ; candidates are ?, _, etc.

    'skip combinator [
        {Match one series item in input, succeeding so long as it's not at END}
        return: "One atom of series input"
            [<opt> any-value!]
    ][
        if tail? input [
            return null
        ]
        set remainder next input
        return quote input.1  ; "core" return protocol
    ]

    ; Historically Rebol used COPY to mean "match across a span of rules and
    ; then copy from the first position to the tail of the match".  That could
    ; have assignments done inside, which extract some values and omit others.
    ; You could thus end up with `y: copy x: ...` and wind up with x and y
    ; being different things, which is not intuitive.
    ;
    ; ACROSS is a synonym for @[...] of a parse rule:
    ;
    ; https://forum.rebol.info/t/1555/8

    'across combinator [
        {Copy from the current parse position through a rule}
        return: "Copied series"
            [<opt> any-series!]
        parser [action!]
    ][
        if [# (remainder)]: parser input [
            return quote copy/part input get remainder  ; "core" protocol
        ]
        return null
    ]

    === INTO KEYWORD ===

    ; Rebol2 had a INTO combinator which only took one argument: a rule to use
    ; when processing the nested input.  There was a popular proposal that
    ; INTO would take a datatype, which would help simplify a common pattern:
    ;
    ;     ahead text! into [some "a"]  ; arity-1 form
    ;     =>
    ;     into text! [some "a"]  ; arity-2 form
    ;
    ; The belief being that wanting to test the type you were going "INTO" was
    ; needed more often than not, and that at worst it would incentivize adding
    ; the type as a comment.  Neither R3-Alpha nor Red adopted this proposal
    ; (but Topaz did).
    ;
    ; UPARSE reframes this not to take just a datatype, but a "value-bearing
    ; rule".  This means you can use it with generated data that is not
    ; strictly resident in the series:
    ;
    ;     uparse "((aaaa)))" [into [between some "(" some ")"] [some "a"]]
    ;
    ; Because any value-bearing rule can be used, SYM-WORD! is also legal,
    ; which lets you break the rules up for legibility (and avoids interpreting
    ; arrays as rules themselves)
    ;
    ;     uparse [| | any any any | | |] [
    ;          content: between some '| some '|
    ;          into @content [some 'any]
    ;     ]

    'into combinator [
        {Perform a recursion into another datatype with a rule}
        return: "Result of the subparser"
            [<opt> any-value!]
        parser [action!]  ; !!! Easier expression of value-bearing parser?
        subparser [action!]
        <local> subseries result
    ][
        if not [subseries (remainder)]: parser input [
            ;
            ; If the parser in the first argument can't get a value to subparse
            ; then we don't process it.
            ;
            ; !!! Review: should we allow non-value-bearing parsers that just
            ; set limits on the input?
            ;
            return null
        ]

        if subseries = [] [
            fail "Cannot parse INTO an invisible synthesized result"
        ]
        assert [quoted? subseries]  ; no true null unless failure

        if not any-series? subseries: my dequote [
            fail "Need ANY-SERIES! datatype for use with INTO in UPARSE"
        ]

        ; If the entirety of the item at the input array is matched by the
        ; supplied parser rule, then we advance past the item.
        ;
        any [
            not [result subseries]: subparser subseries
            not tail? subseries
        ] then [
            return null
        ]
        return result
    ]

    === COLLECT AND KEEP ===

    ; The COLLECT feature was first added by Red.  However, it did not use
    ; rollback across any KEEPs that happened when a parse rule failed, which
    ; makes the feature of limited use.
    ;
    ; This is a kind of lousy implementation that leverages a baked-in
    ; mechanism to manage collecting where the UPARSE holds the collect buffer
    ; and the block combinator is complicit.  It's just to show a first
    ; working option...but a more general architecture for designing features
    ; that want "rollback" is desired.

    'collect combinator [
        return: "Block of collected values"
            [<opt> block!]
        parser [action!]
    ][
        if not state.collecting [
            state.collecting: make block! 10
        ]

        let collect-base: tail state.collecting
        if not [# (remainder)]: parser input [
            ;
            ; Although the block rules roll back, COLLECT might be used with
            ; other combinators that run more than one rule...and one rule
            ; might succeed, then the next fail:
            ;
            ;     uparse "(abc>" [x: collect between keep "(" keep ")"]
            ;
            clear collect-base
            return null
        ]
        ; "core" return protocol, needs to quote
        return (quote copy collect-base, elide clear collect-base)
    ]

    'keep combinator [
        return: "The kept value (same as input)"
            [<opt> any-value!]
        parser [action!]
        /only "Keep blocks as BLOCK!"
        <local> result
    ][
        if not state.collecting [
            fail "UPARSE cannot KEEP with no COLLECT rule in effect"
        ]

        if not [result (remainder)]: parser input [
            return null
        ]
        if result = [] [
            fail "Cannot KEEP an invisible synthesized result"
        ]
        assert [quoted? result]  ; true null if and only if parser failed
        append/(if only ['only]) state.collecting unquote result
        return result
    ]

    === GATHER AND EMIT ===

    ; With gather, the idea is to do more of a "bubble-up" type of strategy
    ; for creating objects with labeled fields.  Also, the idea that PARSE
    ; itself would switch modes.
    ;
    ; !!! A particularly interesting concept that has come up is being able
    ; to "USE" or "IMPORT" an OBJECT! so its fields are local (like a WITH).
    ; This could combine with gather, e.g.
    ;
    ;     import parse [1 "hi"] [
    ;         return gather [emit x: integer!, emit y: text!]
    ;     ]
    ;     print [x "is one and" y "is {hi}"]
    ;
    ; The idea is interesting enough that it suggests being able to EMIT with
    ; no GATHER in effect, and then have the RETURN GATHER semantic.

    'gather combinator [
        return: "The gathered object"
            [<opt> object!]
        parser [action!]
        <local> obj
    ][
        let made-state: did if not state.gathering [
            state.gathering: make block! 10
        ]

        let gather-base: tail state.gathering
        if not [# (remainder)]: parser input [
            ;
            ; Although the block rules roll back, GATHER might be used with
            ; other combinators that run more than one rule...and one rule
            ; might succeed, then the next fail:
            ;
            ;     uparse "(abc>" [x: gather between emit x: "(" emit y: ")"]
            ;
            clear gather-base
            if made-state [
                state.gathering: null
            ]
            return null
        ]
        obj: make object! gather-base
        either made-state [
            state.gathering: null  ; eliminate entirely
        ][
            clear gather-base  ; clear only from the marked position
        ]
        return quote obj  ; "core" return protocol
    ]

    'emit combinator [
        return: "The emitted value"
            [<opt> any-value!]
        'target [set-word!]
        parser [action!]
        <local> result
    ][
        ; !!! Experiment to allow a top-level accrual, to make EMIT more
        ; efficient by becoming the result of the PARSE.
        ;
        if not state.gathering [
            state.gathering: make block! 10
        ]

        if not [result (remainder)]: parser input [
            return null
        ]
        if result = [] [
            fail "Cannot emit an invisible result"
        ]
        assert [quoted? result]  ; should be true null if and only if failed?
        append state.gathering target
        append state.gathering result  ; quoted, due to core return protocol
        return result
    ]

    === SET-WORD! COMBINATOR ===

    ; The concept behind Ren-C's SET-WORD! in PARSE is that parse combinators
    ; don't just update the remainder of the parse input, but they also return
    ; values.  If these appear to the right of a set-word, then the set word
    ; will be assigned on a match.

    set-word! combinator [
        return: "The set value"
            [<opt> any-value!]
        value [set-word!]
        parser [action!]
        <local> result
    ][
        if not [result (remainder)]: parser input [
            ;
            ; A failed rule leaves the set target word at whatever value it
            ; was before the set.
            ;
            return null
        ]

        ; The result that came back uses the "core return protocol".  This
        ; right now means it's [] to denote invisible, null to denote failure,
        ; or a quoted value that needs to be dequoted.  Note that UNQUOTE of
        ; NULL is NULL, as a convenience.
        ;
        if result = [] [
            fail "Cannot assign invisible synthesized rule result"
        ]
        assert [quoted? result]
        set value unquote result  ; value is the SET-WORD!
        return result
    ]

    === TEXT! COMBINATOR ===

    ; For now we just make text act as FIND/MATCH, though this needs to be
    ; sensitive to whether we are operating on blocks or text/binary.
    ;
    ; !!! We presume that it's value-bearing, and gives back the value it
    ; matched against.  If you don't want it, you have to ELIDE it.  Note this
    ; value is the rule in the string and binary case, but the item in the
    ; data in the block case.

    text! combinator [
        return: "The rule series matched against (not input value)"
            [<opt> text!]
        value [text!]
    ][
        case [
            any-array? input [
                if input.1 <> value [
                    return null
                ]
                set remainder next input
                return quote input.1  ; "core" return protocol
            ]

            ; for both of these cases, we have to use the rule, since there's
            ; no isolated value to capture.  Should we copy it?

            any-string? input [
                if not input: find/match/(if state.case 'case) input value [
                    return null
                ]
            ]
            true [
                assert [binary? input]
                if not input: find/match input as binary! value [
                    return null
                ]
            ]
        ]

        set remainder input
        return quote value  ; "core" return protocol
    ]

    === TOKEN! COMBINATOR (currently ISSUE! and CHAR!) ===

    ; The TOKEN! type is an optimized immutable form of string that will
    ; often be able to fit into a cell with no series allocation.  This makes
    ; it good for representing characters, but it can also represent short
    ; strings.  It matches case-sensitively.

    issue! combinator [
        return: "The token matched against (not input value)"
            [<opt> issue!]
        value [issue!]
    ][
        case [
            any-array? input [
                if input.1 = value [
                    set remainder next input
                    return quote input.1  ; "core" return protocol
                ]
                return null
            ]
            any-string? input [
                if set remainder find/match input value [
                    return quote value  ; "core" return protocol
                ]
                return null
            ]
            true [
                assert [binary? input]
                if set remainder find/match input as binary! value [
                    return quote value  ; "core" return protocol
                ]
                return null
            ]
        ]
    ]

    === BINARY! COMBINATOR ===

    ; Arbitrary matching of binary against text is a bit of a can of worms,
    ; because if we AS alias it then that would constrain the binary...which
    ; may not be desirable.  Also you could match partial characters and
    ; then not be able to set a string position.  So we don't do that.

    binary! combinator [
        return: "The binary matched against (not input value)"
            [<opt> binary!]
        value [binary!]
    ][
        case [
            any-array? input [
                if input.1 = value [
                    set remainder next input
                    return quote input.1  ; "core" return protocol
                ]
                return null
            ]
            any-string? input [
                fail "Can't match BINARY! against TEXT! (use AS to alias)"
            ]
            true [
                assert [binary? input]
                if set remainder find/match input value [
                    return quote value  ; "core" return protocol
                ]
                return null
            ]
        ]
    ]

    === GROUP! COMBINATOR ===

    ; GROUP! does not advance the input, just runs the group.  It can return
    ; a value, which is used by value-accepting combinators.  Use ELIDE if
    ; this would be disruptive in terms of a value-bearing BLOCK! rule.

    group! combinator [
        return: "Result of evaluating the group"
            [<opt> any-value!]
        value [group!]
        <local> result
        <static> unique-identity ([])
    ][
        set remainder input

        ; !!! Here we use a bad interim hack to help implement the core
        ; protocol for invisibility.  See notes:
        ;
        ; https://forum.rebol.info/t/1590
        ;
        ; UPARSE is serving as a testbed for incrementally introducing better
        ; ways of handling this sophistication.  The bad way is a first step.
        ;
        if same? unique-identity result: (unique-identity, do value) [
            return []
        ]
        return quote get/any 'result
    ]

    get-block! combinator [
        return: "Undefined at this time"
            [<opt> any-value!]
        value [get-block!]
    ][
        fail "No current meaning for GET-BLOCK! combinator"
    ]

    === BITSET! COMBINATOR ===

    ; There is some question here about whether a bitset used with a BINARY!
    ; can be used to match UTF-8 characters, or only bytes.  This may suggest
    ; a sort of "INTO" switch that could change the way the input is being
    ; viewed, e.g. being able to do INTO BINARY! on a TEXT! (?)

    bitset! combinator [
        return: "The matched input value"
            [<opt> char!]
        value [bitset!]
    ][
        case [
            any-array? input [
                if input.1 = value [
                    set remainder next input
                    return quote input.1  ; "core" return protocol
                ]
            ]
            any-string? input [
                if find value input.1 [
                    set remainder next input
                    return quote input.1  ; "core" return protocol
                ]
            ]
            true [
                assert [binary? input]
                if find value input.1 [
                    set remainder next input
                    return quote input.1  ; "core" return protocol
                ]
            ]
        ]
        return null
    ]

    === QUOTED! COMBINATOR ===

    ; Recognizes the value literally.  Test making it work only on the
    ; ANY-ARRAY! type, just to see if type checking can work.

    quoted! combinator [
        return: "The matched value"
            [<opt> any-value!]
        value [quoted!]
    ][
        if :input.1 = unquote value [
            set remainder next input
            return value  ; leave quoted for "core" return protocol
        ]
        return null
    ]

    === LOGIC! COMBINATOR ===

    ; Handling of LOGIC! in Ren-C replaces the idea of FAIL, because a logic
    ; #[true] is treated as "continue parsing" while #[false] is "rule did
    ; not match".  When combined with GET-GROUP!, this fully replaces the
    ; need for the IF construct.

    logic! combinator [
        return: "True if success, null if failure"
            [<opt> logic!]
        value [logic!]
    ][
        if value [
            set remainder input
            return quote true  ; "core" return protocol
        ]
        return null
    ]

    === INTEGER! COMBINATOR ===

    ; !!! There's currently no way for an integer to be used to represent a
    ; range of matches, e.g. between 1 and 10.  This would need skippable
    ; parameters.  For now we just go for a plain repeat count.

    integer! combinator [
        return: "Last parser result"
            [<opt> any-value!]
        value [integer!]
        parser [action!]
        <local> result
    ][
        result: quote null  ; !!! should `0 skip` resolve to ' like this?
        loop value [
            if not [result input]: parser input [
                return null
            ]
        ]
        set remainder input
        return result
    ]

    === DATATYPE! COMBINATOR ===

    ; Traditionally you could only use a datatype with ANY-ARRAY! types,
    ; but since Ren-C uses UTF-8 Everywhere it makes it practical to merge in
    ; transcoding:
    ;
    ;     >> uparse "{Neat!} 1020" [t: text! i: integer!]
    ;     == "{Neat!} 1020"
    ;
    ;     >> t
    ;     == "Neat!"
    ;
    ;     >> i
    ;     == 1020
    ;
    ; !!! TYPESET! is on somewhat shaky ground as "a thing", so it has to
    ; be thought about as to how `s: any-series!` or `v: any-value!` might
    ; work.  It could be that there's a generic TRANSCODE operation and
    ; then you can filter the result of that.

    datatype! combinator [
        return: "Matched or synthesized value"
            [<opt> any-value!]
        value [datatype!]
        <local> item error
    ][
        either any-array? input [
            if value <> type of input.1 [
                return null
            ]
            set remainder next input
            return quote input.1  ; "core" return protocol
        ][
            any [
                [item (remainder) @error]: transcode input
                value != type of :item
            ] then [
                return null
            ]
            return quote :item  ; "core" return protocol
        ]
    ]

    typeset! combinator [
        return: "Matched or synthesized value"
            [<opt> any-value!]
        value [typeset!]
        <local> item error
    ][
        either any-array? input [
            if not find value (type of input.1) [
                return null
            ]
            set remainder next input
            return quote input.1  ; "core" return protocol
        ][
            any [
                [item (remainder) @error]: transcode input
                not find value (type of :item)
            ] then [
                return null
            ]
            return quote :item  ; "core" return protocol
        ]
    ]

    === SYM-XXX! COMBINATORS ===

    ; The SYM-XXX! combinators are used when you want a variable or expression
    ; to be used literally as the thing matched.
    ;
    ;    x: [some rule]
    ;    uparse [[some rule] [some rule]] [some @x]
    ;
    ; The behavior is basically as if you had tried to match the thing quoted.
    ;
    ;    uparse [[some rule] [some rule]] [some '[some rule]]
    ;
    ; !!! What should be done in the case of NULL?

    sym-word! combinator [
        return: "Exact match"
            [<opt> any-value!]
        value [sym-word!]
    ][
        if :input.1 = get value [
            set remainder next input
            return quote :input.1  ; "core" return protocol
        ]
        return null
    ]

    sym-tuple! combinator [
        return: "Exact match"
            [<opt> any-value!]
        value [sym-tuple!]
    ][
        if :input.1 = get value [
            set remainder next input
            return quote :input.1  ; "core" return protocol
        ]
        return null
    ]

    sym-group! combinator [
        return: "Exact match"
            [<opt> any-value!]
        value [sym-group!]
    ][
        if :input.1 = do value [
            set remainder next input
            return quote :input.1  "core" return protocol
        ]
        return null
    ]

    sym-block! combinator [
        {Make a BLOCK! rule value-bearing as if it were an ACROSS rule}
        return: "Exact match"
            [<opt> any-value!]
        value [sym-block!]
        <local> result parser
    ][
        ; Run the combinator for BLOCK!, and then we will see how far it got.
        ;
        value: as block! value
        parser: :(state.combinators)/(block!)
        if [result (remainder)]: parser state input value [
            return quote copy/part input get remainder  ; "core" return protocol
        ]
        return null
    ]

    === INVISIBLE COMBINATORS ===

    ; If BLOCK! is asked for a result, it will accumulate results from any
    ; result-bearing rules it hits as it goes.  Not all rules give results
    ; by default--such as GROUP! or literals for instance.  If something
    ; gives a result and you do not want it to, use ELIDE.
    ;
    ; !!! Suggestion has made that ELIDE actually be SKIP.  This sounds good,
    ; but would require SKIP as "match next item" having another name.

    'elide combinator [
        {Transform a result-bearing combinator into one that has no result}
        return: "Should be invisible (handling TBD)"
            [<invisible>]
        parser [action!]
    ][
        if not [# (remainder)]: parser input [
            return null
        ]
        return []  ; "core" return protocol, invisible expansion
    ]

    'comment combinator [
        {Comment out an arbitrary amount of PARSE material}
        return: "Should be invisible (handling TBD)"
            [<invisible>]
        'ignored [block! text! tag! issue!]
    ][
        return []  ; "core" return protocol, invisible expansion
    ]

    === BLOCK! COMBINATOR ===

    ; Handling of BLOCK! is the most complex combinator.  It is processed as
    ; a set of alternatives separated by `|`.  The bar is treated specially
    ; and not an "OR combinator" due to semantics, because if arguments to
    ; all the steps were captured in one giant ACTION! that would mean changes
    ; to variables by a GROUP! would not be percieved by later steps...since
    ; once a rule like SOME captures a variable it won't see changes:
    ;
    ; https://forum.rebol.info/t/when-should-parse-notice-changes/1528
    ;
    ; (There is also a performance benefit, since if | was a combinator then
    ; a sequence of 100 alternates would have to build a very large OR
    ; function...rather than being able to build a small function for each
    ; step that could short circuit before the others were needed.)

    block! combinator [
        return: "Last result value"
            [<opt> any-value!]
        value [block!]
        <local> result
    ][
        let rules: value
        let pos: input

        let collect-baseline: tail try state.collecting  ; see COLLECT
        let gather-baseline: tail try state.gathering  ; see GATHER

        ; !!! In the "core return protocol", [] represents invisible (e.g. a
        ; macro that substitutes nothing), while NULL represents "true null"
        ; and QUOTED! represents all "intentional" return values, including
        ; a QUOTED! null which is akin to "null-2".
        ;
        result: []
        while [not tail? rules] [
            if state.verbose [
                print ["RULE:" mold/limit rules 60]
                print ["INPUT:" mold/limit pos 60]
                print "---"
            ]

            if rules.1 = ', [  ; COMMA! is only legal between steps
                rules: my next
                continue
            ]

            if rules.1 = '| [
                ;
                ; Rule alternative was fulfilled.  Base case is a match, e.g.
                ; with input "cde" then [| "ab"] will consider itself to be a
                ; match before any input is consumed, e.g. before the "c".
                ;
                ; But UPARSE has an extra trick up its sleeve with `||`, so
                ; you can have a sequence of alternates within the same block.
                ; scan ahead to see if that's the case.
                ;
                ; !!! This carries a performance penalty, as successful matches
                ; must scan ahead through the whole rule of blocks just as a
                ; failing match would when searching alternates.  Caching
                ; "are there alternates beyond this point" or "are there
                ; sequences beyond this point" could speed that up as flags on
                ; cells if they were available to the internal implementation.
                ;
                catch [  ; use CATCH to continue outer loop
                    let r
                    while [r: rules.1] [
                        rules: my next
                        if r = '|| [
                            input: pos  ; don't roll back past current pos
                            throw <inline-sequence-operator>
                        ]
                    ]
                ] then [
                    continue
                ]

                ; If we didn't find an inline sequencing operator, then the
                ; successful alternate means the whole block is done.
                ;
                set remainder pos
                return :result
            ]

            ; If you hit an inline sequencing operator here then it's the last
            ; alternate in a list.
            ;
            if rules.1 = '|| [
                input: pos  ; don't roll back past current pos
                rules: my next
                continue
            ]

            ; Do one "Parse Step".  This involves turning whatever is at the
            ; next parse position into an ACTION!, then running it.
            ;
            let [action 'rules]: parsify state rules
            let f: make frame! :action
            f.input: pos
            f.remainder: 'pos

            if let temp: do f [
                if temp != [] [  ; don't overwrite result if invisible
                    result: temp
                ]
            ] else [
                result: []  ; forget last result; go back to no-result state

                if state.collecting [  ; toss collected values from this pass
                    if collect-baseline [  ; we marked how far along we were
                        clear collect-baseline
                    ] else [
                        clear state.collecting  ; no mark, must have been empty
                    ]
                ]

                if state.gathering [  ; toss gathered values from this pass
                    if gather-baseline [  ; we marked how far along we were
                        clear gather-baseline
                    ] else [
                        clear state.gathering  ; no mark, must have been empty
                    ]
                ]

                ; If we fail a match, we skip ahead to the next alternate rule
                ; by looking for an `|`, resetting the input position to where
                ; it was when we started.  If there are no more `|` then all
                ; the alternates failed, so return NULL.
                ;
                pos: catch [
                    let r
                    while [r: rules.1] [
                        rules: my next
                        if r = '| [throw input]  ; reset POS

                        ; If we see a sequencing operator after a failed
                        ; alternate, it means we can't consider the alternates
                        ; across that sequencing operator as candidates.  So
                        ; return null just like we would if reaching the end.
                        ;
                        if r = '|| [break]
                    ]
                ] else [
                    set remainder null
                    return null
                ]
            ]
        ]

        ; Deliver errors on block combinator returning no results, e.g.
        ; x: [to "a"] ?
        ;
        set remainder pos
        return :result
    ]
]


non-comma: func [
    value [<opt> any-value!]
][
    ; The concept behind COMMA! is to provide a delimiting between rules.
    ; That is handled by the block combinator.  So if you see a thing like
    ; `[some, "a"]` in PARSIFY, that is just running out of turn.
    ;
    ; It may seem like making a dummy "comma combinator" for the comma
    ; type is a good idea.  But that's an unwanted axis of flexibility
    ; in this model...and also if we defer the error until the combinator
    ; is run, it might never be run.
    ;
    if comma? :value [
        fail "COMMA! can only be run between PARSE steps, not inside them"
    ]
    return :value
]


combinatorize: func [
    return: [frame!]
    advanced: [block!]

    rules [block!]
    state "Parse State" [frame!]
    c "Combinator" [action!]
    /value "Initiating value (if datatype)" [any-value!]
][
    ; Combinators take arguments.  If the arguments are quoted, then they are
    ; taken literally from the rules feed.  If they are not quoted, they will
    ; be another "parser" generated from the rules.
    ;
    ; For instance: CHANGE takes two arguments.  The first is a parser and has
    ; to be constructed with PARSIFY from the rules.  But the replacement is a
    ; literal value, e.g.
    ;
    ;      >> data: "aaabbb"
    ;      >> parse data [change some "a" "literal" some "b"]
    ;      == "literalbbb"
    ;
    ; So we see that CHANGE got SOME "A" turned into a parser action, but it
    ; received "literal" literally.  The definition of the combinator is used
    ; to determine the arguments and which kind they are.

    let f: make frame! :c

    for-each param parameters of :c [
        case [
            param = 'input [
                ; All combinators should have an input.  But the
                ; idea is that we leave this unspecialized.
            ]
            param = '/remainder [
                ; The remainder is a return; responsibility of the caller, also
                ; left unspecialized.
            ]
            param = 'value [
                f.value: value
            ]
            param = 'state [  ; the "state" is currently the UPARSE frame
                f.state: state
            ]
            quoted? param [  ; literal element captured from rules
                let r: non-comma rules.1
                rules: my next

                f.(unquote param): :r
            ]
            refinement? param [
                ; Leave refinements alone, e.g. /only ... a general strategy
                ; would be needed for these if the refinements add parameters
                ; as to how they work.
            ]
            true [  ; another parser to combine with
                ;
                ; !!! At the moment we disallow SET with GROUP!.
                ; This could be more conservative to stop calling
                ; functions.  For now, just work around it.
                ;
                let [temp 'rules]: parsify state rules
                f.(param): :temp
            ]
        ]
    ]

    set advanced rules
    return f
]


identity-combinator: combinator [
    {Combinator used by NULL, e.g. with :(if false [...]) rule patterns}
    return: "Returns NULL (is this right?)"
        [<opt>]
][
    set remainder input
    return quote null  ; "core" return protocol
]


parsify: func [
    {Transform one "step's worth" of rules into a parser combinator action}

    return: "Parser action for input processing corresponding to a full rule"
        [action!]
    advanced: "Rules position advanced past the elements used for the action"
        [block!]

    state "Parse state"
        [frame!]
    rules "Parse rules to (partially) convert to a combinator action"
        [block!]
][
    let r: non-comma rules.1
    rules: my next

    ; Not sure if it's good, but the original GET-GROUP! concept allowed:
    ;
    ;     parse "aaa" [:('some) "a"]
    ;
    ; So evaluate any GET-GROUP!s before the combinator lookup.
    ;
    if get-group? :r [
        r: do r
    ]

    ; As a first step look up any keywords.  There is no WORD! combinator, so
    ; this cannot be abstracted (beyond the GET-GROUP! method above), e.g.
    ; you can't say `s: 'some, parse "aaa" [s "a"]`
    ;
    case [
        null? :r [
            set advanced rules
            return specialize :identity-combinator [state: state]
        ]

        word? :r [
            if let c: select state.combinators r [
                let [f 'rules]: combinatorize rules state :c

                set advanced rules  ; !!! Should `[:advanced]: ...` be ok?
                return make action! f
            ]
            r: get r
        ]

        path? :r [
            let word: ensure word! first r
            if let c: select state.combinators word [
                let [f 'rules]: combinatorize rules state :c
                for-each refinement next as block! r [
                    f/(refinement): #
                ]

                set advanced rules  ; !!! Should `[:advanced]: ...` be ok?
                return make action! f
            ]
            fail ["Unknown combinator:" word]
        ]

        ; !!! Here is where we would let GET-PATH! and GET-WORD! be used to
        ; subvert keywords if SEEK were universally adopted.
    ]

    ; Non-keywords are also handled as combinators, where we just pass the
    ; data value itself to the handler for that type.
    ;
    ; !!! This won't work with INTEGER!, as they are actually rules with
    ; arguments.  Does this mean the block rule has to hardcode handling of
    ; integers, or that when we do these rules they may have skippable types?

    if not let c: select state.combinators kind of :r [
        fail ["Unhandled type in PARSIFY:" kind of :r]
    ]

    let [f 'rules]: combinatorize/value rules state :c r

    set advanced rules

    return make action! f  ; leave INPUT unspecialized
]


=== ENTRY POINTS: UPARSE*, UPARSE, MATCH-UPARSE, and UPARSE? ===

; The historical Redbol PARSE was focused on returning a LOGIC! so that you
; could write `if parse data rules [...]` and easily react to finding out if
; the rules matched the input to completion.
;
; While this bias stuck with UPARSE in the beginning, the deeper power of
; returning the evaluated result made it hugely desirable as the main form
; that owns the word "PARSE".  A version that does not check to completion
; is fundamentally the most powerful, since it can just make HERE the last
; result...and then check that the result is at the tail.  Other variables
; can be set as well.
;
; So this formulates everything on top of a UPARSE* that returns the sythesized
; result

uparse*: func [
    {Process as much of the input as parse rules consume (see also UPARSE)}

    return: "Synthesized value from last match rule, or NULL if rules failed"
        [<opt> any-value!]
    furthest: "Furthest input point reached by the parse"
        [any-series!]

    series "Input series"
        [any-series!]
    @rules "Block of parse rules"
        [block!]
    /verbatim "If last match rule synthesizes NULL, don't return NULL-2"

    /combinators "List of keyword and datatype handlers used for this parse"
        [map!]
    /case "Do case-sensitive matching"

    /verbose "Print some additional debug information"

    <local> collecting (null) gathering (null)
][
    combinators: default [default-combinators]

    ; The COMBINATOR definition makes a function which is hooked with code
    ; that will mark the furthest point reached by any match.
    ;
    if furthest [
        set furthest series
    ]

    ; Each UPARSE operation can have a different set of combinators in
    ; effect.  So it's necessary to need to have some way to get at that
    ; information.  For now, we use the FRAME! of the parse itself as the
    ; way that data is threaded...as it gives access to not just the
    ; combinators, but also the /VERBOSE or other settings...we can add more.
    ;
    let state: binding of 'return

    let f: make frame! :combinators.(block!)
    f.state: state
    f.input: series
    f.value: rules
    f.remainder: let pos

    let synthesized: do f
    if synthesized = [] [  ; !!! Should UPARSE support invisible return?
        synthesized: just '  ; treat it like a match
    ]
    return/(if not verbatim 'vanishable) unquote/isotope synthesized
]

uparse: comment [redescribe [  ; redescribe not working at te moment (?)
    {Process input in the parse dialect, must match to end (see also UPARSE*)}
] ] (
    enclose augment :uparse* [
        /no-auto-gather "Don't implicitly GATHER any un-GATHERed EMITs"
    ] func [f [frame!]] [
        ; Leveraging the core capabilities of UPARSE*, we capture the product
        ; of the passed-in rules, while making the ultimate product of the
        ; compound rule how far the parse managed to get if it succeeds.
        ;
        let gathered: null
        let synthesized
        f.rules: compose [
            ((if not f.no-auto-gather '[
                gathered: gather synthesized:
            ])) (f.rules)
            ((if not f.no-auto-gather '(get/any 'synthesized)))
            elide end  ; we want the rule product
        ]

        let verbatim: f.verbatim  ; can't access after `do f`

        if not let result: quote/isotope do f [
            return null  ; if f.rules failed to match, or end not reached
        ]

        ; !!! This is an experimental feature UPARSE adds on top of UPARSE*
        ; where if there were EMIT-ed things with no gather that we notice,
        ; they are returned as an object, making it possible to USE them
        ; more easily (currently USING).  The macro mechanism (core return)
        ; which would let us inject the [using obj, synthesized] in the
        ; code stream isn't ready, and it inhibits abstraction...but we
        ; might still think it's worth it for UPARSE and people could use
        ; UPARSE* if they had trouble with it.
        ;
        for-each key try gathered [
            return gathered  ; return the object if not empty
        ]
        return @(unquote/isotope result)  ; verbatim was decided by UPARSE*
    ]
)

match-uparse: comment [redescribe [  ; redescribe not working at te moment (?)
    {Process input in the parse dialect, input if match (see also UPARSE*)}
] ] (
    enclose :uparse* func [f [frame!]] [
        ;
        ; Synthesized result will be NULL if no match, or series end if match
        ;
        f.rules: compose [(f.rules) end]

        let input: f.series  ; DO FRAME! invalidates args; cache for returning

        return do f then [input]
    ]
)

uparse?: chain [:uparse* | :then?]


=== REBOL2/R3-ALPHA/RED COMPATIBILITY ===

; One of the early applications of UPARSE is to be able to implement backward
; compatible parse behavior by means of a series of tweaks.

redbol-combinators: copy default-combinators

append redbol-combinators reduce [

    === ANY AND SOME HAVE "NO PROGRESS" CONSTRAINT ===

    ; The no-progress constraint of ANY and SOME are discussed here, they are
    ; believed to perhaps make things harder to understand:
    ;
    ; https://forum.rebol.info/t/any-vs-while-and-not-end/1572/2
    ; https://forum.rebol.info/t/any-vs-many-in-parse-eof-tag-combinators/1540/10
    ;
    ; These compatibility versions are not value-bearing.

    'any combinator [
        {(REDBOL) Any number of matches (including 0), stop if no progress}
        return: "Redbol rules don't return results"
            [void!]
        parser [action!]
        <local> pos
    ][
        cycle [
            any [
                not [# pos]: parser input  ; failed rule => stop successfully
                same? pos input  ; no progress => stop successfully
            ] then [
                set remainder input
                return quote '~any~
            ]
            input: pos
        ]
    ]

    'some combinator [
        {(REDBOL) Must run at least one match, stop if no progress}
        return: "Redbol rules don't return results"
            [void!]
        parser [action!]
        <local> pos
    ][
        any [
            not [# pos]: parser input  ; failed first => stop unsuccessfully
            same? pos input  ; no progress first => stop unsuccessfully
        ] then [
            return null
        ]
        input: pos  ; any future failings won't fail the overall rule
        cycle [
            any [
                not [# pos]: parser input  ; failed rule => stop successfully
                same? pos input  ; no progress => stop successfully
            ] then [
                set remainder input
                return quote '~some~
            ]
            input: pos
        ]
    ]

    === OLD STYLE SET AND COPY COMBINATORS ===

    ; Historical Rebol's PARSE had SET and COPY keywords which would take
    ; a WORD! as their first argument, and then a rule.  This was done because
    ; the SET-WORD! was taken for capturing the parse position into a
    ; variable.  That idea was first overturned by a JavaScript implementation
    ; of Rebol called Topaz, using SET-WORD! for generic captures of data
    ; out of the parse input:
    ;
    ; https://github.com/giesse/red-topaz-parse
    ;
    ; Ren-C goes along with this change, including that the position is
    ; captured by `pos: here` instead of simply by `pos:`.  However, the
    ; concept of how combinators can produce a result to be captured is
    ; rethought.

    'copy combinator [
        {(REDBOL) Copy input series elements into a SET-WORD! or WORD!}
        return: "Redbol rules don't return results"
            [void!]
        'target [word! set-word!]
        parser [action!]
    ][
        if not [# (remainder)]: parser input [
            return null
        ]
        set target copy/part input get remainder
        return quote '~copy~
    ]

    'set combinator [
        {(REDBOL) Take single input element into a SET-WORD! or WORD!}
        return: "Redbol rules don't return results"
            [void!]
        'target [word! set-word!]
        parser [action!]
        <local> pos
    ][
        if not [# (remainder)]: parser input [
            return null
        ]
        if same? (get remainder) input [  ; no advancement gives NONE
            set target null
        ] else [
            set target input.1  ; one unit ahead otherwise
        ]
        return quote '~set~
    ]

    === OLD STYLE SET-WORD! AND GET-WORD! BEHAVIOR ===

    ; This is the handling for sets and gets that are standalone...e.g. not
    ; otherwise quoted as arguments to combinators (like COPY X: SOME "A").
    ; These implement the historical behavior of saving the parse position
    ; into the variable or restoring it.

    set-word! combinator [
        return: "Redbol rules don't return results"
            [void!]
        value [set-word!]
    ][
        set value input
        set remainder input ; don't change position
        return quote '~mark~
    ]

    get-word! combinator [
        return: "Redbol rules don't return results"
            [void!]
        value [get-word!]
    ][
        ; Restriction: seeks must be within the same series.
        ;
        if not same? head input head get value [
            fail "SEEK (via GET-WORD!) in UPARSE must be in the same series"
        ]
        set remainder get value
        return quote '~seek~
    ]

    === OLD-STYLE INTO BEHAVIOR ===

    ; New INTO is arity-2
    ;
    ; https://forum.rebol.info/t/new-more-powerful-arity-2-into-in-uparse/1555

    'into combinator [
        {(REDBOL) Arity-1 Form of Recursion with a rule}
        return: "Redbol rules don't return results"
            [void!]
        subparser [action!]
        <local> subseries pos result
    ][
        if tail? input [
            fail "At END cannot use INTO"
        ]
        if not any-series? subseries: input/1 [
            fail "Need ANY-SERIES! datatype for use with INTO in UPARSE"
        ]

        ; If the entirety of the item at the input array is matched by the
        ; supplied parser rule, then we advance past the item.
        ;
        any [
            not [# subseries]: subparser subseries
            not tail? subseries
        ] then [
            return null
        ]
        set remainder next input
        return quote '~into~
    ]

    === BREAK TEST ===

    'break combinator [
        {Stop an iterating rule but succeed}
        return: "Redbol rules don't return results"
            [void!]
    ][
        fail "BREAK"
    ]

    === OLD-STYLE AND SYNONYM FOR AHEAD ===

    ; AND is a confusing name for AHEAD, doesn't seem to be a lot of value
    ; in carrying that synonym forward in UPARSE.
    ;
    'and :default-combinators.('ahead)

    === OLD-STYLE FAIL INSTRUCTION ===

    ; In Ren-C, the FAIL word is taken to generally relate to raising errors.
    ; PARSE was using it to mean a forced mismatch.
    ;
    ; Ren-C rethinks this so that logic #[false] is used to indicate a match
    ; has failed, and to roll over to the next alternate (if any).  By making
    ; the logic #[true] mean "keep parsing", this allows evaluated expressions
    ; that are substituted into the parse stream via GET-GROUP! to control
    ; whether parsing continues or not.

    'fail combinator [
        {(REDBOL) Interrupt matching with failure}
        return: "Redbol rules don't return results"
            [void!]
    ][
        return null
    ]
]

; Kill off any new combinators.

redbol-combinators.('between): null
redbol-combinators.('gather): null
redbol-combinators.('emit): null

; Red has COLLECT and KEEP, with different semantics--no rollback, and the
; overall parse result changes to the collect result vs. setting a variable.
; That could be emulated.
;
redbol-combinators.('collect): null
redbol-combinators.('keep): null

uparse2: specialize :uparse? [
    combinators: redbol-combinators
]


; !!! This operation will likely take over the name USE.  It is put here since
; the UPARSE tests involve it.
;
using: func [obj [<blank> object!]] [
    add-use-object (binding of 'return) obj
]
