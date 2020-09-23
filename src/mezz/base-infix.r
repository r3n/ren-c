REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Infix operator symbol definitions"
    Rights: {
        Copyright 2012 REBOL Technologies
        Copyright 2012-2017 Rebol Open Source Contributors
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0.
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Purpose: {
        In R3-Alpha, an "OP!" function would gather its left argument greedily
        without waiting for further evaluation, and its right argument would
        stop processing if it hit another "OP!".  This meant that a sequence
        of all infix ops would appear to process left-to-right, e.g.
        `1 + 2 * 3` would be 9.

        Ren-C does not have an "OP!" function type, it just has ACTION!, and
        there is increased fluidity on how actions may optionally interact
        with a left-hand argument (even opportunistically):

        https://forum.rebol.info/t/1156
    }
]


; ARITHMETIC OPERATORS
;
; Note that `/` is actually path containing two elements, both of which are
; BLANK! so they do not render.  A special mechanism through a word binding
; hidden in the cell allows it to dispatch.  See Startup_Slash_1_Symbol()

+: enfixed :add
-: enfixed :subtract
*: enfixed :multiply
-slash-1-: enfixed :divide  ; TBD: make `/: enfixed :divide` act equivalently


; SET OPERATORS

and+: enfixed :intersect
or+: enfixed :union
xor+: enfixed :difference


; COMPARISON OPERATORS
;
; !!! See discussion about the future of comparison operators:
; https://forum.rebol.info/t/349

=: enfixed :equal?
<>: enfixed :not-equal?
<: enfixed :lesser?
>: enfixed :greater?

>=: enfixed :greater-or-equal?
=<: <=: enfixed :equal-or-lesser?  ; https://forum.rebol.info/t/349/11
!=: enfixed :not-equal?  ; http://www.rebol.net/r3blogs/0017.html
==: enfixed :strict-equal?  ; !!! https://forum.rebol.info/t/349
!==: enfixed :strict-not-equal?  ; !!! bad pairing, most would think !=

=?: enfixed :same?



=>: enfixed :lambda  ; quick function generator


; <- is the SHOVE operator.  It grabs whatever is on the left and uses it as
; the first argument to whatever operation is on its right hand side.  It
; adopts the parameter convention of the right.  If the right's first argument
; is evaluative and the left is a SET-WORD! or SET-PATH!, it will grab the
; value of that and then run the assignment after the function is done.
;
; While both <- and -> are enfix operations, the -> variation does not follow
; the rules of enfix completion:
;
;     >> 10 <- lib/= 5 + 5
;     ** Script Error: + does not allow logic! for its value1 argument
;
;     >> 10 -> lib/= 5 + 5
;     == #[true]
;
<-: enfixed :shove/enfix
->: enfixed :shove


; The -- and ++ operators were deemed too "C-like", so ME was created to allow
; `some-var: me + 1` or `some-var: me / 2` in a generic way.  They share code
; with SHOVE, so it's folded into the implementation of that.

me: enfixed redescribe [
    {Update variable using it as the left hand argument to an enfix operator}
](
    :shove/set/enfix  ; /ENFIX so `x: 1 | x: me + 1 * 10` is 20, not 11
)

my: enfixed redescribe [
    {Update variable using it as the first argument to a prefix operator}
](
    :shove/set
)


; These constructs used to be enfix to complete their left hand side.  Yet
; that form of completion was only one expression's worth, when they wanted
; to allow longer runs of evaluation.  "Invisible functions" (those which
; `return: []`) permit a more flexible version of the mechanic.

<|: tweak copy :eval-all 'postpone on
|>: tweak enfixed :shove 'postpone on
||: :once-bar
