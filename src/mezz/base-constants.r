REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "REBOL 3 Boot Base: Constants and Equates"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Note: {
        This code is evaluated just after actions, natives, sysobj, and other lower
        levels definitions. This file intializes a minimal working environment
        that is used for the rest of the boot.
    }
]

; NOTE: The system is not fully booted at this point, so only simple
; expressions are allowed. Anything else will crash the boot.

; Standard constants

on:  true
off: false
yes: true
no:  false
zero: 0

; Special values

sys: system/contexts/sys
lib: system/contexts/lib

; Char constants

nul: NUL:  #"^(NULL)"
space:     #" "
sp: SP:    space
backspace: #"^(BACK)"
bs: BS:    backspace
tab:       #"^-"
newline:   #"^/"
newpage:   #"^l"
slash:     #"/"
backslash: #"\"
escape:    #"^(ESC)"
cr: CR:    #"^M"
lf: LF:    newline

; Function synonyms

to-logic: :did
to-value: :try
min: :minimum
max: :maximum
abs: :absolute

; Note: NULL symbol is in lib context slot 1, is initialized on boot
blank: _   ; e.g. sometimes `return blank` reads better than `return _`

null-1?: :else?

; Note: VOID would have to be a function that returned ~void~, since plain
; `void: ~void~` would error on access.  In practice, this causes confusion
; because `type of get/any 'void` winds up being ACTION!...and that isn't
; really worth it.  Plus, encouraging people to label their voids with why
; the value is "no good" helps places that receive those voids untangle the
; reason for its existence.  So a VOID function is avoided in the box.
