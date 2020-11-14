; %source-comment.test.reb
;
; Test for semicolon-based comments.  One key difference in Ren-C with these
; comments is that a comment must have space between it and another token,
; otherwise the semicolon is considered part of the token:
;
;     >> #; ; comment after a one-codepoint issue for `;`
;     == #;
;
;     >> # ; comment after a zero-codepoint issue
;     == #
;
;     >> 'a ; comment after a quoted word
;     == 'a
;
;     >> 'a; illegal quoted word
;     ** Syntax Error: invalid "word" -- "a;"
;
; This differs from Rebol2 and Red.

(
    issue: load "#; ; comment after a one-codepoint issue"
    did all [
        issue? issue
        1 = length of issue
        59 = codepoint of issue
        ";" = to text! issue
    ]
)(
    issue: load "# ; comment after a zero-codepoint issue"
    did all [
        issue? issue
        0 = length of issue
        0 = codepoint of issue
        'illegal-zero-byte = (trap [to text! issue])/id
    ]
)

(
    q-word: load "'a ; comment after a quoted word"
    did all [
        quoted? q-word
        (first [a]) = unquote q-word
        'a = unquote q-word
        "a" = to text! unquote q-word
    ]
)(
    'scan-invalid = (trap [load "'a; illegal quoted word"])/id
)

; Semicolons are technically legal in URL (though many things that auto-scan
; code to find URLs in text won't include period, semicolon, quotes...)
(
    url: load "http://abc;"
    http://abc; = url
)

(
    b: load ";"
    did all [
        b = []
        not new-line? b
    ]
)
