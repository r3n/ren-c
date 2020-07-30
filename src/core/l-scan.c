//
//  File: %l-scan.c
//  Summary: "lexical analyzer for source to binary translation"
//  Section: lexical
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2019 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Rebol's lexical scanner was implemented as hand-coded C, as opposed to
// using a more formal grammar and generator.  This makes the behavior hard
// to formalize, though some attempts have been made to do so:
//
// http://rgchris.github.io/Rebol-Notation/
//
// Because Red is implemented using Rebol, it has a more abstract definition
// in the sense that it uses PARSE rules:
//
// https://github.com/red/red/blob/master/lexer.r
//
// It would likely be desirable to bring more formalism and generativeness
// to Rebol's scanner; though the current method of implementation was
// ostensibly chosen for performance.
//

#include "sys-core.h"


//
// Maps each character to its lexical attributes, using
// a frequency optimized encoding.
//
// UTF8: The values C0, C1, F5 to FF never appear.
//
const REBYTE Lex_Map[256] =
{
    /* 00 EOF */    LEX_DELIMIT|LEX_DELIMIT_END,
    /* 01     */    LEX_DEFAULT,
    /* 02     */    LEX_DEFAULT,
    /* 03     */    LEX_DEFAULT,
    /* 04     */    LEX_DEFAULT,
    /* 05     */    LEX_DEFAULT,
    /* 06     */    LEX_DEFAULT,
    /* 07     */    LEX_DEFAULT,
    /* 08 BS  */    LEX_DEFAULT,
    /* 09 TAB */    LEX_DEFAULT,
    /* 0A LF  */    LEX_DELIMIT|LEX_DELIMIT_LINEFEED,
    /* 0B     */    LEX_DEFAULT,
    /* 0C PG  */    LEX_DEFAULT,
    /* 0D CR  */    LEX_DELIMIT|LEX_DELIMIT_RETURN,
    /* 0E     */    LEX_DEFAULT,
    /* 0F     */    LEX_DEFAULT,

    /* 10     */    LEX_DEFAULT,
    /* 11     */    LEX_DEFAULT,
    /* 12     */    LEX_DEFAULT,
    /* 13     */    LEX_DEFAULT,
    /* 14     */    LEX_DEFAULT,
    /* 15     */    LEX_DEFAULT,
    /* 16     */    LEX_DEFAULT,
    /* 17     */    LEX_DEFAULT,
    /* 18     */    LEX_DEFAULT,
    /* 19     */    LEX_DEFAULT,
    /* 1A     */    LEX_DEFAULT,
    /* 1B     */    LEX_DEFAULT,
    /* 1C     */    LEX_DEFAULT,
    /* 1D     */    LEX_DEFAULT,
    /* 1E     */    LEX_DEFAULT,
    /* 1F     */    LEX_DEFAULT,

    /* 20     */    LEX_DELIMIT|LEX_DELIMIT_SPACE,
    /* 21 !   */    LEX_WORD,
    /* 22 "   */    LEX_DELIMIT|LEX_DELIMIT_DOUBLE_QUOTE,
    /* 23 #   */    LEX_SPECIAL|LEX_SPECIAL_POUND,
    /* 24 $   */    LEX_SPECIAL|LEX_SPECIAL_DOLLAR,
    /* 25 %   */    LEX_SPECIAL|LEX_SPECIAL_PERCENT,
    /* 26 &   */    LEX_WORD,
    /* 27 '   */    LEX_SPECIAL|LEX_SPECIAL_APOSTROPHE,
    /* 28 (   */    LEX_DELIMIT|LEX_DELIMIT_LEFT_PAREN,
    /* 29 )   */    LEX_DELIMIT|LEX_DELIMIT_RIGHT_PAREN,
    /* 2A *   */    LEX_WORD,
    /* 2B +   */    LEX_SPECIAL|LEX_SPECIAL_PLUS,
    /* 2C ,   */    LEX_SPECIAL|LEX_SPECIAL_COMMA,
    /* 2D -   */    LEX_SPECIAL|LEX_SPECIAL_MINUS,
    /* 2E .   */    LEX_SPECIAL|LEX_SPECIAL_PERIOD,
    /* 2F /   */    LEX_DELIMIT|LEX_DELIMIT_SLASH,

    /* 30 0   */    LEX_NUMBER|0,
    /* 31 1   */    LEX_NUMBER|1,
    /* 32 2   */    LEX_NUMBER|2,
    /* 33 3   */    LEX_NUMBER|3,
    /* 34 4   */    LEX_NUMBER|4,
    /* 35 5   */    LEX_NUMBER|5,
    /* 36 6   */    LEX_NUMBER|6,
    /* 37 7   */    LEX_NUMBER|7,
    /* 38 8   */    LEX_NUMBER|8,
    /* 39 9   */    LEX_NUMBER|9,
    /* 3A :   */    LEX_SPECIAL|LEX_SPECIAL_COLON,
    /* 3B ;   */    LEX_DELIMIT|LEX_DELIMIT_SEMICOLON,
    /* 3C <   */    LEX_SPECIAL|LEX_SPECIAL_LESSER,
    /* 3D =   */    LEX_WORD,
    /* 3E >   */    LEX_SPECIAL|LEX_SPECIAL_GREATER,
    /* 3F ?   */    LEX_WORD,

    /* 40 @   */    LEX_SPECIAL|LEX_SPECIAL_AT,
    /* 41 A   */    LEX_WORD|10,
    /* 42 B   */    LEX_WORD|11,
    /* 43 C   */    LEX_WORD|12,
    /* 44 D   */    LEX_WORD|13,
    /* 45 E   */    LEX_WORD|14,
    /* 46 F   */    LEX_WORD|15,
    /* 47 G   */    LEX_WORD,
    /* 48 H   */    LEX_WORD,
    /* 49 I   */    LEX_WORD,
    /* 4A J   */    LEX_WORD,
    /* 4B K   */    LEX_WORD,
    /* 4C L   */    LEX_WORD,
    /* 4D M   */    LEX_WORD,
    /* 4E N   */    LEX_WORD,
    /* 4F O   */    LEX_WORD,

    /* 50 P   */    LEX_WORD,
    /* 51 Q   */    LEX_WORD,
    /* 52 R   */    LEX_WORD,
    /* 53 S   */    LEX_WORD,
    /* 54 T   */    LEX_WORD,
    /* 55 U   */    LEX_WORD,
    /* 56 V   */    LEX_WORD,
    /* 57 W   */    LEX_WORD,
    /* 58 X   */    LEX_WORD,
    /* 59 Y   */    LEX_WORD,
    /* 5A Z   */    LEX_WORD,
    /* 5B [   */    LEX_DELIMIT|LEX_DELIMIT_LEFT_BRACKET,
    /* 5C \   */    LEX_SPECIAL|LEX_SPECIAL_BACKSLASH,
    /* 5D ]   */    LEX_DELIMIT|LEX_DELIMIT_RIGHT_BRACKET,
    /* 5E ^   */    LEX_WORD,
    /* 5F _   */    LEX_SPECIAL|LEX_SPECIAL_BLANK,

    /* 60 `   */    LEX_WORD,
    /* 61 a   */    LEX_WORD|10,
    /* 62 b   */    LEX_WORD|11,
    /* 63 c   */    LEX_WORD|12,
    /* 64 d   */    LEX_WORD|13,
    /* 65 e   */    LEX_WORD|14,
    /* 66 f   */    LEX_WORD|15,
    /* 67 g   */    LEX_WORD,
    /* 68 h   */    LEX_WORD,
    /* 69 i   */    LEX_WORD,
    /* 6A j   */    LEX_WORD,
    /* 6B k   */    LEX_WORD,
    /* 6C l   */    LEX_WORD,
    /* 6D m   */    LEX_WORD,
    /* 6E n   */    LEX_WORD,
    /* 6F o   */    LEX_WORD,

    /* 70 p   */    LEX_WORD,
    /* 71 q   */    LEX_WORD,
    /* 72 r   */    LEX_WORD,
    /* 73 s   */    LEX_WORD,
    /* 74 t   */    LEX_WORD,
    /* 75 u   */    LEX_WORD,
    /* 76 v   */    LEX_WORD,
    /* 77 w   */    LEX_WORD,
    /* 78 x   */    LEX_WORD,
    /* 79 y   */    LEX_WORD,
    /* 7A z   */    LEX_WORD,
    /* 7B {   */    LEX_DELIMIT|LEX_DELIMIT_LEFT_BRACE,
    /* 7C |   */    LEX_SPECIAL|LEX_SPECIAL_BAR,
    /* 7D }   */    LEX_DELIMIT|LEX_DELIMIT_RIGHT_BRACE,
    /* 7E ~   */    LEX_WORD, // !!! once belonged to LEX_SPECIAL
    /* 7F DEL */    LEX_DEFAULT,

    /* Odd Control Chars */
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,    /* 80 */
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,

    /* Alternate Chars */
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,

    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,

    // C0, C1
    LEX_UTFE,LEX_UTFE,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,

    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,

    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,

    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_UTFE,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_WORD,
    LEX_WORD,LEX_WORD,LEX_WORD,LEX_UTFE
};

#ifdef LOWER_CASE_BYTE
//
// Maps each character to its upper case value.  Done this way for speed.
// Note the odd cases in last block.
//
const REBYTE Upper_Case[256] =
{
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
     16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
     32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
     48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,

     64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
     80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
     96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
     80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,123,124,125,126,127,

    128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
    // some up/low cases mod 16 (not mod 32)
    144,145,146,147,148,149,150,151,152,153,138,155,156,141,142,159,
    160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
    176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,

    192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
    208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
    192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
    208,209,210,211,212,213,214,247,216,217,218,219,220,221,222,159
};


// Maps each character to its lower case value.  Done this way for speed.
// Note the odd cases in last block.
//
const REBYTE Lower_Case[256] =
{
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
     16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
     32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
     48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,

     64, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
    112,113,114,115,116,117,118,119,120,121,122, 91, 92, 93, 94, 95,
     96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
    112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,

    128,129,130,131,132,133,134,135,136,137,154,139,140,157,158,143,
    // some up/low cases mod 16 (not mod 32)
    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,255,
    160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
    176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,

    224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
    240,241,242,243,244,245,246,215,248,249,250,251,252,253,254,223,
    224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
    240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};
#endif


//
//  Scan_UTF8_Char_Escapable: C
//
// Scan a char, handling ^A, ^/, ^(null), ^(1234)
//
// Returns the numeric value for char, or nullptr for errors.
// 0 is a legal codepoint value which may be returned.
//
// Advances the cp to just past the last position.
//
// test: to-integer load to-binary mold to-char 1234
//
static const REBYTE *Scan_UTF8_Char_Escapable(REBUNI *out, const REBYTE *bp)
{
    const REBYTE *cp;
    REBYTE lex;

    REBYTE c = *bp;

    if (c >= 0x80) {  // multibyte sequence
        if (not (bp = Back_Scan_UTF8_Char(out, bp, nullptr)))
            return nullptr;
        return bp + 1;  // Back_Scan advances one less than the full encoding
    }

    bp++;

    if (c != '^') {
        *out = c;
        return bp;
    }

    c = *bp;  // Must be ^ escaped char
    bp++;

    switch (c) {

    case 0:
        *out = 0;
        break;

    case '/':
        *out = LF;
        break;

    case '^':
        *out = c;
        break;

    case '-':
        *out = '\t';  // tab character
        break;

    case '!':
        *out = '\036';  // record separator
        break;

    case '(':  // ^(tab) ^(1234)
        cp = bp; // restart location
        *out = 0;

        // Check for hex integers ^(1234)
        while ((lex = Lex_Map[*cp]) > LEX_WORD) {
            c = lex & LEX_VALUE;
            if (c == 0 and lex < LEX_NUMBER)
                break;
            *out = (*out << 4) + c;
            cp++;
        }
        if (*cp == ')') {
            cp++;
            return cp;
        }

        // Check for identifiers
        for (c = 0; c < ESC_MAX; c++) {
            if ((cp = Match_Bytes(bp, cb_cast(Esc_Names[c])))) {
                if (cp != nullptr and *cp == ')') {
                    bp = cp + 1;
                    *out = Esc_Codes[c];
                    return bp;
                }
            }
        }
        return nullptr;

    default:
        *out = c;

        c = UP_CASE(c);
        if (c >= '@' and c <= '_')
            *out = c - '@';
        else if (c == '~')
            *out = 0x7f; // special for DEL
        else {
            // keep original `c` value before UP_CASE (includes: ^{ ^} ^")
        }
    }

    return bp;
}


//
//  Scan_Quote_Push_Mold: C
//
// Scan a quoted string, handling all the escape characters.  e.g. an input
// stream might have "a^(1234)b" and need to turn "^(1234)" into the right
// UTF-8 bytes for that codepoint in the string.
//
static const REBYTE *Scan_Quote_Push_Mold(
    REB_MOLD *mo,
    const REBYTE *src,
    SCAN_STATE *ss
){
    Push_Mold(mo);

    REBUNI term; // pick termination
    if (*src == '{')
        term = '}';
    else {
        assert(*src == '"');
        term = '"';
    }
    ++src;

    REBINT nest = 0;
    REBLEN lines = 0;
    while (*src != term or nest > 0) {
        REBUNI c = *src;

        switch (c) {
          case '\0':
            // TEXT! literals can have embedded "NUL"s if escaped, but an
            // actual `\0` codepoint in the scanned text is not legal.
            //
            return nullptr;

          case '^':
            if ((src = Scan_UTF8_Char_Escapable(&c, src)) == NULL)
                return NULL;
            --src;  // unlike Back_Scan_XXX, no compensation for ++src later
            break;

          case '{':
            if (term != '"')
                ++nest;
            break;

          case '}':
            if (term != '"' and nest > 0)
                --nest;
            break;

          case CR: {
            //
            // !!! Historically CR LF was scanned as just an LF.  While a
            // tolerant mode of the scanner might be created someday, for
            // the moment we are being more prescriptive.
            //
            enum Reb_Strmode strmode = STRMODE_NO_CR;
            if (strmode == STRMODE_CRLF_TO_LF) {
                if (src[1] == LF) {
                    ++src;
                    c = LF;
                    goto linefeed;
                }
            }
            else
                assert(strmode == STRMODE_NO_CR);
            fail (Error_Illegal_Cr(src, ss->begin)); }

          case LF:
          linefeed:
            if (term == '"')
                return nullptr;
            ++lines;
            break;

          default:
            if (c >= 0x80) {
                if ((src = Back_Scan_UTF8_Char(&c, src, nullptr)) == nullptr)
                    return nullptr;
            }
        }

        ++src;

        if (c == '\0')  // e.g. ^(00) or ^@
            fail (Error_Illegal_Zero_Byte_Raw());  // legal CHAR!, not string

        Append_Codepoint(mo->series, c);
    }

    ss->line += lines;

    ++src; // Skip ending quote or brace.
    return src;
}


//
//  Scan_Item_Push_Mold: C
//
// Scan as UTF8 an item like a file.  Handles *some* forms of escaping, which
// may not be a great idea (see notes below on how URL! moved away from that)
//
// Returns continuation point or NULL for error.  Puts result into the
// temporary mold buffer as UTF-8.
//
const REBYTE *Scan_Item_Push_Mold(
    REB_MOLD *mo,
    const REBYTE *bp,
    const REBYTE *ep,
    REBYTE opt_term,  // '\0' if file like %foo - '"' if file like %"foo bar"
    const REBYTE *opt_invalids
){
    assert(opt_term < 128);  // method below doesn't search for high chars

    Push_Mold(mo);

    while (bp != ep and *bp != opt_term) {
        REBUNI c = *bp;

        if (c == '\0')
            break;  // End of stream

        if ((opt_term == '\0') and IS_WHITE(c))
            break;  // Unless terminator like '"' %"...", any whitespace ends

        if (c < ' ')
            return nullptr;  // Ctrl characters not valid in filenames, fail

        // !!! The branches below do things like "forces %\foo\bar to become
        // %/foo/bar".  But it may be that this kind of lossy scanning is a
        // poor idea, and it's better to preserve what the user entered then
        // have FILE-TO-LOCAL complain it's malformed when turning to a
        // STRING!--or be overridden explicitly to be lax and tolerate it.
        //
        // (URL! has already come under scrutiny for these kinds of automatic
        // translations that affect round-trip copy and paste, and it seems
        // applicable to FILE! too.)
        //
        if (c == '\\') {
            c = '/';
        }
        else if (c == '%') { // Accept %xx encoded char:
            REBYTE decoded;
            bp = Scan_Hex2(&decoded, bp + 1);
            if (bp == nullptr)
                return nullptr;
            c = decoded;
            --bp;
        }
        else if (c == '^') {  // Accept ^X encoded char:
            if (bp + 1 == ep)
                return nullptr;  // error if nothing follows ^
            if (not (bp = Scan_UTF8_Char_Escapable(&c, bp)))
                return nullptr;
            if (opt_term == '\0' and IS_WHITE(c))
                break;
            --bp;
        }
        else if (c >= 0x80) { // Accept UTF8 encoded char:
            if (not (bp = Back_Scan_UTF8_Char(&c, bp, 0)))
                return nullptr;
        }
        else if (opt_invalids and strchr(cs_cast(opt_invalids), c)) {
            //
            // Is char as literal valid? (e.g. () [] etc.)
            // Only searches ASCII characters.
            //
            return nullptr;
        }

        ++bp;

        if (c == '\0')  // e.g. ^(00) or ^@
            fail (Error_Illegal_Zero_Byte_Raw());  // legal CHAR!, not string

        Append_Codepoint(mo->series, c);
    }

    if (*bp != '\0' and *bp == opt_term)
        ++bp;

    return bp;
}


//
//  Skip_Tag: C
//
// Skip the entire contents of a tag, including quoted strings.
// The argument points to the opening '<'.  nullptr is returned on errors.
//
static const REBYTE *Skip_Tag(const REBYTE *cp)
{
    if (*cp == '<')
        ++cp;

    while (*cp != '\0' and *cp != '>') {
        if (*cp == '"') {
            cp++;
            while (*cp != '\0' and *cp != '"')
                ++cp;
            if (*cp == '\0')
                return nullptr;
        }
        cp++;
    }

    if (*cp != '\0')
        return cp + 1;

    return nullptr;
}


//
//  Update_Error_Near_For_Line: C
//
// The NEAR information in an error is typically expressed in terms of loaded
// Rebol code.  Scanner errors have historically used the NEAR not to tell you
// where the LOAD that is failing is in Rebol, but to form a string of the
// "best place" to report the textual error.
//
// While this is probably a bad overloading of NEAR, it is being made more
// clear that this is what's happening for the moment.
//
static void Update_Error_Near_For_Line(
    REBCTX *error,
    SCAN_STATE *ss,
    REBLEN line,
    const REBYTE *line_head
){
    // Skip indentation (don't include in the NEAR)
    //
    const REBYTE *cp = line_head;
    while (IS_LEX_SPACE(*cp))
        ++cp;

    // Find end of line to capture in error message
    //
    REBLEN len = 0;
    const REBYTE *bp = cp;
    while (not ANY_CR_LF_END(*cp)) {
        cp++;
        len++;
    }

    // Put the line count and the line's text into a string.
    //
    // !!! This should likely be separated into an integer and a string, so
    // that those processing the error don't have to parse it back out.
    //
    DECLARE_MOLD (mo);
    Push_Mold(mo);
    Append_Ascii(mo->series, "(line ");
    Append_Int(mo->series, line);
    Append_Ascii(mo->series, ") ");
    Append_Utf8(mo->series, cs_cast(bp), len);

    ERROR_VARS *vars = ERR_VARS(error);
    Init_Text(&vars->nearest, Pop_Molded_String(mo));

    Init_Word(&vars->file, ss->file);
    Init_Integer(&vars->line, ss->line);
}


//
//  Error_Syntax: C
//
// Catch-all scanner error handler.  Reports the name of the token that gives
// the complaint, and gives the substring of the token's text.  Populates
// the NEAR field of the error with the "current" line number and line text,
// e.g. where the end point of the token is seen.
//
static REBCTX *Error_Syntax(SCAN_STATE *ss, enum Reb_Token token) {
    //
    // The scanner code has `bp` and `ep` locals which mirror ss->begin and
    // ss->end.  However, they get out of sync.  If they are updated, they
    // should be sync'd before calling here, since it's used to find the
    // range of text to report.
    //
    // !!! Would it be safer to go to ss->b and ss->e, or something similar,
    // to get almost as much brevity and not much less clarity than bp and
    // ep, while avoiding the possibility of the state getting out of sync?
    //
    assert(ss->begin and not IS_POINTER_TRASH_DEBUG(ss->begin));
    assert(ss->end and not IS_POINTER_TRASH_DEBUG(ss->end));
    assert(ss->end >= ss->begin);

    DECLARE_LOCAL (token_name);
    Init_Text(token_name, Make_String_UTF8(Token_Names[token]));

    DECLARE_LOCAL (token_text);
    Init_Text(
        token_text,
        Make_Sized_String_UTF8(
            cs_cast(ss->begin), cast(REBLEN, ss->end - ss->begin)
        )
    );

    REBCTX *error = Error_Scan_Invalid_Raw(token_name, token_text);
    Update_Error_Near_For_Line(error, ss, ss->line, ss->line_head);
    return error;
}


//
//  Error_Missing: C
//
// Caused by code like: `load "( abc"`.
//
// Note: This error is useful for things like multi-line input, because it
// indicates a state which could be reconciled by adding more text.  A
// better form of this error would walk the scan state stack and be able to
// report all the unclosed terms.
//
static REBCTX *Error_Missing(SCAN_LEVEL *level, char wanted) {
    DECLARE_LOCAL (expected);
    Init_Text(expected, Make_Codepoint_String(wanted));

    REBCTX *error = Error_Scan_Missing_Raw(expected);

    // We have two options of where to implicate the error...either the start
    // of the thing being scanned, or where we are now (or, both).  But we
    // only have the start line information for GROUP! and BLOCK!...strings
    // don't cause recursions.  So using a start line on a string would point
    // at the block the string is in, which isn't as useful.
    //
    if (wanted == ')' or wanted == ']')
        Update_Error_Near_For_Line(
            error,
            level->ss,
            level->start_line,
            level->start_line_head
        );
    else
        Update_Error_Near_For_Line(
            error,
            level->ss,
            level->ss->line,
            level->ss->line_head
        );
    return error;
}


//
//  Error_Extra: C
//
// For instance, `load "abc ]"`
//
static REBCTX *Error_Extra(SCAN_STATE *ss, char seen) {
    DECLARE_LOCAL (unexpected);
    Init_Text(unexpected, Make_Codepoint_String(seen));

    REBCTX *error = Error_Scan_Extra_Raw(unexpected);
    Update_Error_Near_For_Line(error, ss, ss->line, ss->line_head);
    return error;
}


//
//  Error_Mismatch: C
//
// For instance, `load "( abc ]"`
//
// Note: This answer would be more useful for syntax highlighting or other
// applications if it would point out the locations of both points.  R3-Alpha
// only pointed out the location of the start token.
//
static REBCTX *Error_Mismatch(SCAN_LEVEL *level, char wanted, char seen) {
    REBCTX *error = Error_Scan_Mismatch_Raw(rebChar(wanted), rebChar(seen));
    Update_Error_Near_For_Line(
        error,
        level->ss,
        level->start_line,
        level->start_line_head
    );
    return error;
}


//
//  Prescan_Token: C
//
// This function updates `ss->begin` to skip past leading whitespace.  If the
// first character it finds after that is a LEX_DELIMITER (`"`, `[`, `)`, `{`,
// etc. or a space/newline) then it will advance the end position to just past
// that one character.  For all other leading characters, it will advance the
// end pointer up to the first delimiter class byte (but not include it.)
//
// If the first character is not a delimiter, then this routine also gathers
// a quick "fingerprint" of the special characters that appeared after it, but
// before a delimiter was found.  This comes from unioning LEX_SPECIAL_XXX
// flags of the bytes that are seen (plus LEX_SPECIAL_WORD if any legal word
// bytes were found in that range.)
//
// For example, if the input were `$#foobar[@`
//
// - The flags LEX_SPECIAL_POUND and LEX_SPECIAL_WORD would be set.
// - $ wouldn't add LEX_SPECIAL_DOLLAR (it is the first character)
// - @ wouldn't add LEX_SPECIAL_AT (it's after the LEX_CLASS_DELIMITER '['
//
// Note: The reason the first character's lexical class is not considered is
// because it's important to know it *exactly*, so the caller will use
// GET_LEX_CLASS(ss->begin[0]).  Fingerprinting just helps accelerate further
// categorization.
//
static REBLEN Prescan_Token(SCAN_STATE *ss)
{
    assert(IS_POINTER_TRASH_DEBUG(ss->end));  // prescan only uses ->begin

    const REBYTE *cp = ss->begin;
    REBLEN flags = 0;

    while (IS_LEX_SPACE(*cp))  // skip whitespace (if any)
        ++cp;
    ss->begin = cp;  // don't count leading whitespace as part of token

    while (true) {
        switch (GET_LEX_CLASS(*cp)) {
          case LEX_CLASS_DELIMIT:
            if (cp == ss->begin) {
                //
                // Include the delimiter if it is the only character we
                // are returning in the range (leave it out otherwise)
                //
                ss->end = cp + 1;

                // Note: We'd liked to have excluded LEX_DELIMIT_END, but
                // would require a GET_LEX_VALUE() call to know to do so.
                // Locate_Token_May_Push_Mold() does a `switch` on that,
                // so it can subtract this addition back out itself.
            }
            else
                ss->end = cp;
            return flags;

          case LEX_CLASS_SPECIAL:
            if (cp != ss->begin) {
                // As long as it isn't the first character, we union a flag
                // in the result mask to signal this special char's presence
                SET_LEX_FLAG(flags, GET_LEX_VALUE(*cp));
            }
            ++cp;
            break;

          case LEX_CLASS_WORD:
            //
            // If something is in LEX_CLASS_SPECIAL it gets set in the flags
            // that are returned.  But if any member of LEX_CLASS_WORD is
            // found, then a flag will be set indicating that also.
            //
            SET_LEX_FLAG(flags, LEX_SPECIAL_WORD);
            while (IS_LEX_WORD_OR_NUMBER(*cp))
                ++cp;
            break;

          case LEX_CLASS_NUMBER:
            while (IS_LEX_NUMBER(*cp))
                ++cp;
            break;
        }
    }

    DEAD_END;
}


//
//  Locate_Token_May_Push_Mold: C
//
// Find the beginning and end character pointers for the next token in the
// scanner state.  If the scanner is being fed variadically by a list of UTF-8
// strings and REBVAL pointers, then any Rebol values encountered will be
// spliced into the array being currently gathered by pushing them to the data
// stack (as tokens can only be located in UTF-8 strings encountered).
//
// The scan state will be updated so that `ss->begin` has been moved past any
// leading whitespace that was pending in the buffer.  `ss->end` will hold the
// conclusion at a delimiter.  The calculated token will be returned.
//
// The TOKEN_XXX type returned will correspond directly to a Rebol datatype
// if it isn't an ANY-ARRAY! (e.g. TOKEN_INTEGER for INTEGER! or TOKEN_STRING
// for STRING!).  When a block or group delimiter is found it will indicate
// that, e.g. TOKEN_BLOCK_BEGIN will be returned to indicate the scanner
// should recurse... or TOKEN_GROUP_END which will signal the end of a level
// of recursion.
//
// TOKEN_END is returned if end of input is reached.
//
// Newlines that should be internal to a non-ANY-ARRAY! type are included in
// the scanned range between the `begin` and `end`.  But newlines that are
// found outside of a string are returned as TOKEN_NEWLINE.  (These are used
// to set the CELL_FLAG_NEWLINE_BEFORE bits on the next value.)
//
// Determining the end point of token types that need escaping requires
// processing (for instance `{a^}b}` can't see the first close brace as ending
// the string).  To avoid double processing, the routine decodes the string's
// content into MOLD_BUF for any quoted form to be used by the caller.  It's
// overwritten in successive calls, and is only done for quoted forms (e.g.
// %"foo" will have data in MOLD_BUF but %foo will not.)
//
// !!! This is a somewhat weird separation of responsibilities, that seems to
// arise from a desire to make "Scan_XXX" functions independent of the
// "Locate_Token_May_Push_Mold" function.  But if the work of locating the
// value means you have to basically do what you'd do to read it into a REBVAL
// anyway, why split it?  This is especially true now that the variadic
// splicing pushes values directly from this routine.
//
// Error handling is limited for most types, as an additional phase is needed
// to load their data into a REBOL value.  Yet if a "cheap" error is
// incidentally found during this routine without extra cost to compute, it
// can fail here.
//
// Examples with ss's (B)egin (E)nd and return value:
//
//     foo: baz bar => TOKEN_SET
//     B   E
//
//     [quick brown fox] => TOKEN_BLOCK_BEGIN
//     B
//      E
//
//     "brown fox]" => TOKEN_WORD
//      B    E
//
//     $10AE.20 sent => fail()
//     B       E
//
//     {line1\nline2}  => TOKEN_STRING (content in MOLD_BUF)
//     B             E
//
//     \n{line2} => TOKEN_NEWLINE (newline is external)
//     BB
//       E
//
//     %"a ^"b^" c" d => TOKEN_FILE (content in MOLD_BUF)
//     B           E
//
//     %a-b.c d => TOKEN_FILE (content *not* in MOLD_BUF)
//     B     E
//
//     \0 => TOKEN_END
//     BB
//     EE
//
// Note: The reason that the code is able to use byte scanning over UTF-8
// encoded source is because all the characters that dictate the tokenization
// are currently in the ASCII range (< 128).
//
static enum Reb_Token Locate_Token_May_Push_Mold(
    REB_MOLD *mo,
    SCAN_LEVEL *level
){
    SCAN_STATE *ss = level->ss;
    TRASH_POINTER_IF_DEBUG(ss->end);  // this routine should set ss->end

  acquisition_loop:
    //
    // If a non-variadic scan of a UTF-8 string is being done, then ss->vaptr
    // is nullptr and ss->begin will be set to the data to scan.  A variadic
    // scan will start ss->begin at nullptr also.
    //
    // Each time a string component being scanned gets exhausted, ss->begin
    // will be set to nullptr and this loop is run to see if there's more
    // input to be processed.
    //
    while (not ss->begin) {
        if (not ss->feed)  // not a variadic va_list-based scan...
            return TOKEN_END;  // ...so end of utf-8 input was *the* end

        const void *p = va_arg(*ss->feed->vaptr, const void*);
        if (not p or Detect_Rebol_Pointer(p) != DETECTED_AS_UTF8) {
            //
            // If it's not a UTF-8 string we don't know how to handle it.
            // Don't want to repeat complex value decoding logic here, so
            // call common routine.
            //
            // !!! This is a recursion, since it is the function that calls
            // the scanner in the first place when it saw a UTF-8 pointer.
            // This should be protected against feeding through instructions
            // and causing another recursion (it shouldn't do so now).  This
            // suggests we might need a better way of doing things, but it
            // shows the general gist for now.
            //
            Detect_Feed_Pointer_Maybe_Fetch(ss->feed, p, false);

            if (IS_END(ss->feed->value))
                return TOKEN_END;

            Derelativize(DS_PUSH(), ss->feed->value, ss->feed->specifier);

            if (level->newline_pending) {
                level->newline_pending = false;
                SET_CELL_FLAG(DS_TOP, NEWLINE_BEFORE);
            }
        }
        else {  // It's UTF-8, so have to scan it ordinarily.

            ss->begin = cast(const REBYTE*, p);  // breaks the loop...

            // If we're using a va_list, we start the scan with no C string
            // pointer to serve as the beginning of line for an error message.
            // wing it by just setting the line pointer to whatever the start
            // of the first UTF-8 string fragment we see.
            //
            // !!! A more sophisticated debug mode might "reify" the va_list
            // as a BLOCK! before scanning, which might be able to give more
            // context for the error-causing input.
            //
            if (not ss->line_head) {
                assert(ss->feed->vaptr != nullptr);
                assert(not level->start_line_head);
                level->start_line_head = ss->line_head = ss->begin;
            }
         }
    }

    REBLEN flags = Prescan_Token(ss);  // sets ->begin, ->end

    const REBYTE *cp = ss->begin;

    enum Reb_Token token;  // only set if falling through to `scan_word`

    switch (GET_LEX_CLASS(*cp)) {
      case LEX_CLASS_DELIMIT:
        switch (GET_LEX_VALUE(*cp)) {
          case LEX_DELIMIT_SPACE:
            panic ("Prescan_Token did not skip whitespace");

          case LEX_DELIMIT_SEMICOLON:  // ; begin comment
            while (not ANY_CR_LF_END(*cp))
                ++cp;
            if (*cp == '\0')
                return TOKEN_END;  // `load ";"` is [] with no newline on tail
            if (*cp == LF)
                goto delimit_line_feed;
            assert(*cp == CR);
            goto delimit_return;

          case LEX_DELIMIT_RETURN:
          delimit_return: {
            //
            // !!! Ren-C is attempting to rationalize and standardize Rebol
            // on line feeds only.  If for some reason we wanted a tolerant
            // mode, that tolerance would go here.  Note that this code does
            // not cover the case of CR that are embedded inside multi-line
            // string literals.
            //
            enum Reb_Strmode strmode = STRMODE_NO_CR;  // ss->strmode ?
            if (strmode == STRMODE_CRLF_TO_LF) {
                if (cp[1] == LF) {
                    ++cp;
                    goto delimit_line_feed;
                }
            }
            else
                assert(strmode == STRMODE_NO_CR);

            REBCTX *error = Error_Illegal_Cr(cp, ss->begin);
            Update_Error_Near_For_Line(error, ss, ss->line, ss->line_head);
            fail (error); }

          case LEX_DELIMIT_LINEFEED:
          delimit_line_feed:
            ss->line++;
            ss->end = cp + 1;
            return TOKEN_NEWLINE;

          case LEX_DELIMIT_LEFT_BRACKET:  // [BLOCK] begin
            return TOKEN_BLOCK_BEGIN;

          case LEX_DELIMIT_RIGHT_BRACKET:  // [BLOCK] end
            return TOKEN_BLOCK_END;

          case LEX_DELIMIT_LEFT_PAREN:  // (GROUP) begin
            return TOKEN_GROUP_BEGIN;

          case LEX_DELIMIT_RIGHT_PAREN:  // (GROUP) end
            return TOKEN_GROUP_END;

          case LEX_DELIMIT_DOUBLE_QUOTE:  // "QUOTES"
            cp = Scan_Quote_Push_Mold(mo, cp, ss);
            goto check_str;

          case LEX_DELIMIT_LEFT_BRACE:  // {BRACES}
            cp = Scan_Quote_Push_Mold(mo, cp, ss);

          check_str:
            if (cp) {
                ss->end = cp;
                return TOKEN_STRING;
            }
            // try to recover at next new line...
            cp = ss->begin + 1;
            while (not ANY_CR_LF_END(*cp))
                ++cp;
            ss->end = cp;
            if (ss->begin[0] == '"')
                fail (Error_Missing(level, '"'));
            if (ss->begin[0] == '{')
                fail (Error_Missing(level, '}'));
            panic ("Invalid string start delimiter");

          case LEX_DELIMIT_RIGHT_BRACE:
            fail (Error_Extra(ss, '}'));

          case LEX_DELIMIT_SLASH:  // a /REFINEMENT-style PATH!
            assert(*cp == '/');
            ++cp;
            ss->end = cp;
            return TOKEN_PATH;

          case LEX_DELIMIT_END:
            //
            // We've reached the end of this string token's content.  By
            // putting nullptr in ss->begin, that cues the acquisition loop
            // to check if there's a variadic pointer in effect to see if
            // there's more content yet to come.
            //
            ss->begin = nullptr;
            TRASH_POINTER_IF_DEBUG(ss->end);
            goto acquisition_loop;

          case LEX_DELIMIT_UTF8_ERROR:
            fail (Error_Syntax(ss, TOKEN_WORD));

          default:
            panic ("Invalid LEX_DELIMIT class");
        }

      case LEX_CLASS_SPECIAL:
        if (
            HAS_LEX_FLAG(flags, LEX_SPECIAL_AT)  // @ anywhere but at the head
            and *cp != '<'  // want <foo="@"> to be a TAG!, not an EMAIL!
            and *cp != '\''  // want '@foo to be a SYM-WORD!
        ){
            if (*cp == '@')  // consider `@a@b`, `@@`, etc. ambiguous
                fail (Error_Syntax(ss, TOKEN_EMAIL));
            return TOKEN_EMAIL;
        }

      next_lex_special:

        switch (GET_LEX_VALUE(*cp)) {
          case LEX_SPECIAL_AT:  // the case where @ is actually at the head
            if (cp[1] == '(') {
                ss->end = cp + 2;  // whole token should be `@(`
                return TOKEN_SYM_GROUP_BEGIN;
            }
            if (cp[1] == '[') {
                ss->end = cp + 2;  // whole token should be `@[`
                return TOKEN_SYM_BLOCK_BEGIN;
            }
            ++cp;  // skip @
            token = TOKEN_SYM;
            goto scanword;

          case LEX_SPECIAL_PERCENT:  // %filename
            cp = ss->end;
            if (*cp == '"') {
                cp = Scan_Quote_Push_Mold(mo, cp, ss);
                if (not cp)
                    fail (Error_Syntax(ss, TOKEN_FILE));
                ss->end = cp;
                return TOKEN_FILE;
            }
            while (*cp == '/') {  // deal with path delimiter
                cp++;
                while (IS_LEX_NOT_DELIMIT(*cp))
                    ++cp;
            }
            ss->end = cp;
            return TOKEN_FILE;

          case LEX_SPECIAL_COLON:  // :word :12 (time)
            if (cp[1] == '(') {
                ss->end = cp + 2;  // whole token should be `:(`
                return TOKEN_GET_GROUP_BEGIN;
            }
            if (cp[1] == '[') {
                ss->end = cp + 2;  // whole token should be `:[`
                return TOKEN_GET_BLOCK_BEGIN;
            }
            if (IS_LEX_NUMBER(cp[1]))
                return TOKEN_TIME;

            if (ONLY_LEX_FLAG(flags, LEX_SPECIAL_WORD))
                return TOKEN_GET;  // "common case"

            if (cp[1] == '\'')
                fail (Error_Syntax(ss, TOKEN_WORD));

            // Various special cases of < << <> >> > >= <=
            if (cp[1] == '<' or cp[1] == '>') {
                cp++;
                if (cp[1] == '<' or cp[1] == '>' or cp[1] == '=')
                    ++cp;
                if (not IS_LEX_DELIMIT(cp[1]))
                    fail (Error_Syntax(ss, TOKEN_GET));
                ss->end = cp + 1;
                return TOKEN_GET;
            }
            token = TOKEN_GET;
            ++cp; // skip ':'
            goto scanword;

          case LEX_SPECIAL_APOSTROPHE:
            while (*cp == '\'')  // get sequential apostrophes as one token
                ++cp;
            ss->end = cp;
            return TOKEN_APOSTROPHE;

          case LEX_SPECIAL_COMMA:  // ,123
          case LEX_SPECIAL_PERIOD:  // .123 .123.456.789 */
            SET_LEX_FLAG(flags, (GET_LEX_VALUE(*cp)));
            if (IS_LEX_NUMBER(cp[1]))
                goto num;
            if (GET_LEX_VALUE(*cp) != LEX_SPECIAL_PERIOD)
                fail (Error_Syntax(ss, TOKEN_WORD));
            token = TOKEN_WORD;
            goto scanword;

          case LEX_SPECIAL_GREATER:
            if (IS_LEX_DELIMIT(cp[1]))
                return TOKEN_WORD;
            if (cp[1] == '>') {
                if (IS_LEX_DELIMIT(cp[2]))
                    return TOKEN_WORD;
                fail (Error_Syntax(ss, TOKEN_WORD));
            }
            goto special_lesser;

          case LEX_SPECIAL_LESSER:
          special_lesser:;
            if (
                IS_LEX_ANY_SPACE(cp[1])
                or cp[1] == ']' or cp[1] == ')' or cp[1] == 0
            ){
                return TOKEN_WORD;  // changed for </tag>
            }
            if (
                (cp[0] == '<' and cp[1] == '<')
                or cp[1] == '='
                or cp[1] == '>'
            ){
                if (IS_LEX_DELIMIT(cp[2]))
                    return TOKEN_WORD;
                fail (Error_Syntax(ss, TOKEN_WORD));
            }
            if (
                cp[0] == '<' and (cp[1] == '-' or cp[1] == '|')
                and (IS_LEX_DELIMIT(cp[2]) or IS_LEX_ANY_SPACE(cp[2]))
            ){
                return TOKEN_WORD;  // "<|" and "<-"
            }
            if (GET_LEX_VALUE(*cp) == LEX_SPECIAL_GREATER)
                fail (Error_Syntax(ss, TOKEN_WORD));

            cp = Skip_Tag(cp);
            if (not cp)
                fail (Error_Syntax(ss, TOKEN_TAG));
            ss->end = cp;
            return TOKEN_TAG;

          case LEX_SPECIAL_PLUS:  // +123 +123.45 +$123
          case LEX_SPECIAL_MINUS:  // -123 -123.45 -$123
            if (HAS_LEX_FLAG(flags, LEX_SPECIAL_AT))
                return TOKEN_EMAIL;
            if (HAS_LEX_FLAG(flags, LEX_SPECIAL_DOLLAR))
                return TOKEN_MONEY;
            if (HAS_LEX_FLAG(flags, LEX_SPECIAL_COLON)) {
                cp = Skip_To_Byte(cp, ss->end, ':');
                if (cp and (cp + 1) != ss->end)  // 12:34
                    return TOKEN_TIME;
                cp = ss->begin;
                if (cp[1] == ':') {  // +: -:
                    token = TOKEN_WORD;
                    goto scanword;
                }
            }
            cp++;
            if (IS_LEX_NUMBER(*cp))
                goto num;
            if (IS_LEX_SPECIAL(*cp)) {
                if ((GET_LEX_VALUE(*cp)) >= LEX_SPECIAL_PERIOD)
                    goto next_lex_special;
                if (*cp == '+' or *cp == '-') {
                    token = TOKEN_WORD;
                    goto scanword;
                }
                if (
                    *cp == '>'
                    and (IS_LEX_DELIMIT(cp[1]) or IS_LEX_ANY_SPACE(cp[1]))
                ){
                    return TOKEN_WORD;  // Special exemption for ->
                }
                fail (Error_Syntax(ss, TOKEN_WORD));
            }
            token = TOKEN_WORD;
            goto scanword;

          case LEX_SPECIAL_BAR:
            if (
                cp[1] == '>'
                and (IS_LEX_DELIMIT(cp[2]) or IS_LEX_ANY_SPACE(cp[2]))
            ){
                return TOKEN_WORD;  // for `|>`
            }
            token = TOKEN_WORD;
            goto scanword;

          case LEX_SPECIAL_BLANK:
            //
            // `_` standalone should become a BLANK!, so if followed by a
            // delimiter or space.  However `_a_` and `a_b` are left as
            // legal words (at least for the time being).
            //
            if (IS_LEX_DELIMIT(cp[1]) or IS_LEX_ANY_SPACE(cp[1]))
                return TOKEN_BLANK;
            token = TOKEN_WORD;
            goto scanword;

          case LEX_SPECIAL_POUND:
          pound:
            cp++;
            if (*cp == '[') {
                ss->end = ++cp;
                return TOKEN_CONSTRUCT;
            }
            if (*cp == '"') {  // CHAR #"C"
                REBUNI dummy;
                cp++;
                cp = Scan_UTF8_Char_Escapable(&dummy, cp);
                if (cp and *cp == '"') {
                    ss->end = cp + 1;
                    return TOKEN_CHAR;
                }
                // try to recover at next new line...
                cp = ss->begin + 1;
                while (not ANY_CR_LF_END(*cp))
                    ++cp;
                ss->end = cp;
                fail (Error_Syntax(ss, TOKEN_CHAR));
            }
            if (*cp == '{') {  // BINARY #{12343132023902902302938290382}
                ss->end = ss->begin;  // save start
                ss->begin = cp;
                cp = Scan_Quote_Push_Mold(mo, cp, ss);
                ss->begin = ss->end;  // restore start
                if (cp) {
                    ss->end = cp;
                    return TOKEN_BINARY;
                }
                // try to recover at next new line...
                cp = ss->begin + 1;
                while (not ANY_CR_LF_END(*cp))
                    ++cp;
                ss->end = cp;

                // !!! This was Error_Syntax(ss, TOKEN_BINARY), but if we use
                // the same error as for an unclosed string the console uses
                // that to realize the binary may be incomplete.  It may also
                // have bad characters in it, but that would be detected by
                // the caller, so we mention the missing `}` first.)
                //
                fail (Error_Missing(level, '}'));
            }
            if (cp - 1 == ss->begin)
                return TOKEN_ISSUE;

            fail (Error_Syntax(ss, TOKEN_INTEGER));

          case LEX_SPECIAL_DOLLAR:
            if (HAS_LEX_FLAG(flags, LEX_SPECIAL_AT)) {
                return TOKEN_EMAIL;
            }
            return TOKEN_MONEY;

          default:
            fail (Error_Syntax(ss, TOKEN_WORD));
        }

      case LEX_CLASS_WORD:
        if (ONLY_LEX_FLAG(flags, LEX_SPECIAL_WORD))
            return TOKEN_WORD;
        token = TOKEN_WORD;
        goto scanword;

      case LEX_CLASS_NUMBER:  // Note: "order of tests is important"
      num:;
        if (flags == 0)  // simple integer
            return TOKEN_INTEGER;
        if (HAS_LEX_FLAG(flags, LEX_SPECIAL_AT))
            return TOKEN_EMAIL;
        if (HAS_LEX_FLAG(flags, LEX_SPECIAL_POUND)) {
            if (cp == ss->begin) {  // no +2 +16 +64 allowed
                if (
                    (
                        cp[0] == '6'
                        and cp[1] == '4'
                        and cp[2] == '#'
                        and cp[3] == '{'
                    ) or (
                        cp[0] == '1'
                        and cp[1] == '6'
                        and cp[2] == '#'
                        and cp[3] == '{'
                    ) // rare
                ) {
                    cp += 2;
                    goto pound;
                }
                if (cp[0] == '2' and cp[1] == '#' and cp[2] == '{') {
                    cp++;
                    goto pound;  // base-2 binary, "very rare"
                }
            }
            fail (Error_Syntax(ss, TOKEN_INTEGER));
        }
        if (HAS_LEX_FLAG(flags, LEX_SPECIAL_COLON))  // 12:34
            return TOKEN_TIME;
        if (HAS_LEX_FLAG(flags, LEX_SPECIAL_PERIOD)) {
            // 1.2 1.2.3 1,200.3 1.200,3 1.E-2
            if (Skip_To_Byte(cp, ss->end, 'x'))
                return TOKEN_PAIR;
            cp = Skip_To_Byte(cp, ss->end, '.');
            // Note: no comma in bytes
            if (
                not HAS_LEX_FLAG(flags, LEX_SPECIAL_COMMA)
                and Skip_To_Byte(cp + 1, ss->end, '.')
            ){
                return TOKEN_TUPLE;
            }
            return TOKEN_DECIMAL;
        }
        if (HAS_LEX_FLAG(flags, LEX_SPECIAL_COMMA)) {
            if (Skip_To_Byte(cp, ss->end, 'x'))
                return TOKEN_PAIR;
            return TOKEN_DECIMAL;  // 1,23;
        }
        if (HAS_LEX_FLAG(flags, LEX_SPECIAL_POUND)) { // -#123 2#1010
            if (
                HAS_LEX_FLAGS(
                    flags,
                    ~(
                        LEX_FLAG(LEX_SPECIAL_POUND)
                        | LEX_FLAG(LEX_SPECIAL_PERIOD)
                        | LEX_FLAG(LEX_SPECIAL_APOSTROPHE)
                    )
                )
            ){
                fail (Error_Syntax(ss, TOKEN_INTEGER));
            }
            if (HAS_LEX_FLAG(flags, LEX_SPECIAL_PERIOD))
                return TOKEN_TUPLE;
            return TOKEN_INTEGER;
        }

        // "Note: cannot detect dates of the form 1/2/1998 because they
        // may appear within a path, where they are not actually dates!
        // Special parsing is required at the next level up."
        //
        for (; cp != ss->end; cp++) {
            // what do we hit first? 1-AUG-97 or 123E-4
            if (*cp == '-')
                return TOKEN_DATE;  // 1-2-97 1-jan-97
            if (*cp == 'x' or *cp == 'X')
                return TOKEN_PAIR;  // 320x200
            if (*cp == 'E' or *cp == 'e') {
                if (Skip_To_Byte(cp, ss->end, 'x'))
                    return TOKEN_PAIR;
                return TOKEN_DECIMAL;  // 123E4
            }
            if (*cp == '%')
                return TOKEN_PERCENT;
        }
        if (HAS_LEX_FLAG(flags, LEX_SPECIAL_APOSTROPHE))  // 1'200
            return TOKEN_INTEGER;
        fail (Error_Syntax(ss, TOKEN_INTEGER));

      default:
        break;  // panic after switch, so no cases fall through accidentally
    }

    panic ("Invalid LEX class");

  scanword:;  // `token` should be set, compiler warnings should catch if not

    if (HAS_LEX_FLAG(flags, LEX_SPECIAL_COLON)) { // word:  url:words
        if (token != TOKEN_WORD)  // only valid with WORD (not set or lit)
            return token;
        cp = Skip_To_Byte(cp, ss->end, ':');
        assert(*cp == ':');
        if (cp[1] != '/' and Lex_Map[cp[1]] < LEX_SPECIAL) {
            // a valid delimited word SET?
            if (HAS_LEX_FLAGS(
                flags,
                ~LEX_FLAG(LEX_SPECIAL_COLON) & LEX_WORD_FLAGS
            )){
                fail (Error_Syntax(ss, TOKEN_WORD));
            }
            return TOKEN_SET;
        }
        cp = ss->end;  // then, must be a URL
        while (*cp == '/') {  // deal with path delimiter
            cp++;
            while (IS_LEX_NOT_DELIMIT(*cp) or not IS_LEX_DELIMIT_HARD(*cp))
                ++cp;
        }
        ss->end = cp;
        return TOKEN_URL;
    }
    if (HAS_LEX_FLAG(flags, LEX_SPECIAL_AT))
        return TOKEN_EMAIL;
    if (HAS_LEX_FLAG(flags, LEX_SPECIAL_DOLLAR))
        return TOKEN_MONEY;
    if (HAS_LEX_FLAGS(flags, LEX_WORD_FLAGS))
        fail (Error_Syntax(ss, TOKEN_WORD));  // has non-word chars (eg % \ )

    if (HAS_LEX_FLAG(flags, LEX_SPECIAL_LESSER)) {
        // Allow word<tag> and word</tag> but not word< word<= word<> etc.

        if (*cp == '=' and cp[1] == '<' and IS_LEX_DELIMIT(cp[2]))
            return TOKEN_WORD;  // enable `=<`

        cp = Skip_To_Byte(cp, ss->end, '<');
        if (
            cp[1] == '<' or cp[1] == '>' or cp[1] == '='
            or IS_LEX_SPACE(cp[1])
            or (cp[1] != '/' and IS_LEX_DELIMIT(cp[1]))
        ){
            fail (Error_Syntax(ss, token));
        }
        ss->end = cp;
    }
    else if (HAS_LEX_FLAG(flags, LEX_SPECIAL_GREATER)) {
        if (*cp == '=' and cp[1] == '>' and IS_LEX_DELIMIT(cp[2]))
            return TOKEN_WORD;  // enable `=>`
        fail (Error_Syntax(ss, token));
    }

    return token;
}


//
//  Init_Va_Scan_Level_Core: C
//
// Initialize a scanner state structure, using variadic C arguments.
//
void Init_Va_Scan_Level_Core(
    SCAN_LEVEL *level,
    SCAN_STATE *ss,
    REBSTR *file,
    REBLIN line,
    const REBYTE *opt_begin,  // preload the scanner outside the va_list
    struct Reb_Feed *feed
){
    level->ss = ss;
    ss->feed = feed;

    ss->begin = opt_begin;  // if null, Locate_Token's first fetch from vaptr
    TRASH_POINTER_IF_DEBUG(ss->end);

    ss->file = file;
    ss->depth = 0;

    // !!! Splicing REBVALs into a scan as it goes creates complexities for
    // error messages based on line numbers.  Fortunately the splice of a
    // REBVAL* itself shouldn't cause a fail()-class error if there's no
    // data corruption, so it should be able to pick up *a* line head before
    // any errors occur...it just might not give the whole picture when used
    // to offer an error message of what's happening with the spliced values.
    //
    level->start_line_head = ss->line_head = nullptr;
    level->start_line = ss->line = line;
    level->mode_char = '\0';
    level->newline_pending = false;
    level->opts = 0;
}


//
//  Init_Scan_Level: C
//
void Init_Scan_Level(
    SCAN_LEVEL *out,
    SCAN_STATE *ss,
    REBSTR *file,
    REBLIN line,
    const REBYTE *utf8,
    REBLEN limit  // !!! limit feature not implemented in R3-Alpha
){
    out->ss = ss;

    assert(utf8[limit] == '\0');  // if limit used, make sure it was the end
    UNUSED(limit);

    ss->feed = nullptr;  // signal Locate_Token this isn't a variadic scan
    ss->begin = utf8;
    TRASH_POINTER_IF_DEBUG(ss->end);

    ss->file = file;
    ss->feed = nullptr;
    ss->depth = 0;

    out->mode_char = '\0';
    out->start_line_head = ss->line_head = utf8;
    out->start_line = ss->line = line;
    out->newline_pending = false;
    out->opts = 0;
}


//
//  Scan_Head: C
//
// Search text for a REBOL header.  It is distinguished as the word REBOL
// followed by a '[' (they can be separated only by lines and comments).
//
// There can be nothing on the line before the header.  Also, if a '['
// precedes the header, then note its position (for embedded code).
//
// Returns:
//     0 if no header,
//     1 if header,
//    -1 if embedded header (inside []).
//
// The ss begin pointer is updated to point to the header block.
// The ss structure is updated to point to the beginning of the source text.
// Keep track of line-count.
//
static REBINT Scan_Head(SCAN_STATE *ss)
{
    const REBYTE *rebol = nullptr;  // start of the REBOL word
    const REBYTE *bracket = nullptr;  // optional [ just before REBOL
    const REBYTE *cp = ss->begin;
    REBLEN count = ss->line;

    while (true) {
        while (IS_LEX_SPACE(*cp))  // skip white space
            ++cp;

        switch (*cp) {
          case '[':
            if (rebol) {
                ss->begin = ++cp;
                ss->line = count;
                return (bracket ? -1 : 1);
            }
            bracket = cp++;
            break;

          case 'R':
          case 'r':
            if (Match_Bytes(cp, cb_cast(Str_REBOL))) {
                rebol = cp;
                cp += 5;
                break;
            }
            cp++;
            bracket = nullptr;  // prior '[' was a red herring
            goto semicolon;

        case ';':
        semicolon:
            goto skipline;

          case 0:
            return 0;

        default:  // everything else...
            if (not ANY_CR_LF_END(*cp))
                rebol = bracket = nullptr;

          skipline:
            while (not ANY_CR_LF_END(*cp))
                ++cp;
            if (*cp == CR and cp[1] == LF)
                ++cp;
            if (*cp != '\0')
                ++cp;
            ++count;
            break;
        }
    }

    DEAD_END;
}

#define CELL_FLAG_BLANK_MARKED_GET NODE_FLAG_MARKED


static REBARR *Scan_Child_Array(SCAN_LEVEL *level, REBYTE mode_char);

//
//  Scan_To_Stack: C
//
// Scans values to the data stack, based on a mode_char.  This mode can be
// ']', ')', or '/' to indicate the processing type...or '\0'.
//
// If the source bytes are "1" then it will be the array [1]
// If the source bytes are "[1]" then it will be the array [[1]]
//
// BLOCK! and GROUP! use fairly ordinary recursions of this routine to make
// arrays.  PATH! scanning is a bit trickier...it starts after an element was
// scanned and is immediately followed by a `/`.  The stack pointer is marked
// to include that previous element, and a recursive call to Scan_To_Stack()
// collects elements so long as a `/` is seen between them.  When space is
// reached, the element that was seen prior to the `/` is integrated into a
// path to replace it in the scan of the array the path is in.  (e.g. if the
// prior element was a GET-WORD!, the scan becomes a GET-PATH!...if the final
// element is a SET-WORD!, the scan becomes a SET-PATH!)
//
// Return value is always nullptr, since output is sent to the data stack.
// (It only has a return value because it may be called by rebRescue(), and
// that's the convention it uses.)
//
REBVAL *Scan_To_Stack(SCAN_LEVEL *level) {
    DECLARE_MOLD (mo);

    if (C_STACK_OVERFLOWING(&mo))
        Fail_Stack_Overflow();

    SCAN_STATE *ss = level->ss;

    const bool just_once = did (level->opts & SCAN_FLAG_NEXT);
    if (just_once)
        level->opts &= ~SCAN_FLAG_NEXT;  // recursion loads an entire BLOCK!

    REBLEN lit_depth = 0;

    enum Reb_Token token;

  loop:

    while (true) {
        Drop_Mold_If_Pushed(mo);
        token = Locate_Token_May_Push_Mold(mo, level);
        if (token == TOKEN_END)
            break;

        assert(ss->begin and ss->end and ss->begin < ss->end);

        const REBYTE *bp = ss->begin;
        const REBYTE *ep = ss->end;
        REBLEN len = cast(REBLEN, ep - bp);

        ss->begin = ss->end;  // accept token

        switch (token) {
          case TOKEN_NEWLINE:
            level->newline_pending = true;
            ss->line_head = ep;
            continue;

          case TOKEN_BLANK:
            Init_Blank(DS_PUSH());
            ++bp;
            break;

          case TOKEN_SYM: {  // !!! Similar to TOKEN_GET, try unifying
            ++bp;
            goto token_set; }

          case TOKEN_GET:
            if (ep[-1] == ':') {
                if (ep[0] == '/') {  // e.g. :/foo
                    Init_Blank(DS_PUSH());  // let blank be head of path

                    // !!! Ugly hack due to lack of GET-BLANK! (which we
                    // probably do not want...)  Since path scanning converts
                    // values in the first slot that are GET-XXX! to mean the
                    // overall path is a GET-PATH!, we need another signal.
                    //
                    SET_CELL_FLAG(DS_TOP, BLANK_MARKED_GET);

                    break;  // mode will be changed to '/'
                }
                if (len == 1 or level->mode_char != '/')
                    goto syntax_error;
                --len;
                --ss->end;
            }
            bp++;
            goto token_set;

          case TOKEN_SET:
          token_set:
            len--;
            if (level->mode_char == '/' and token == TOKEN_SET) {
                token = TOKEN_WORD;  // will be a PATH_SET
                ss->end--;  // put ':' back on end but not beginning
            }
            goto token_word;

          case TOKEN_WORD:
          token_word:
            if (len == 0) {
                --bp;
                goto syntax_error;
            }

            Init_Any_Word(
                DS_PUSH(),
                KIND_OF_WORD_FROM_TOKEN(token),
                Intern_UTF8_Managed(bp, len)
            );
            break;

          case TOKEN_ISSUE:
            if (ep != Scan_Issue(DS_PUSH(), bp + 1, len - 1))
                goto syntax_error;
            break;

          case TOKEN_APOSTROPHE: {
            if (lit_depth != 0)  // e.g. `' '`, nothing seen since last one
                Quotify(Init_Nulled(DS_PUSH()), lit_depth);

            assert(ss->end > bp);
            lit_depth = ss->end - bp;

            if (not ANY_CR_LF_END(*ss->begin))  // more to come...(maybe)
                goto loop;  // so wrap next value

            Quotify(Init_Nulled(DS_PUSH()), lit_depth);
            lit_depth = 0;
            goto loop; }  // wrap next value

          case TOKEN_SYM_GROUP_BEGIN:
          case TOKEN_SYM_BLOCK_BEGIN:
          case TOKEN_GET_GROUP_BEGIN:
          case TOKEN_GET_BLOCK_BEGIN:
            if (ep[-1] == ':') {
                if (len == 1 or level->mode_char != '/')
                    goto syntax_error;
                --len;
                --ss->end;
            }
            bp++;
            goto token_array_begin;

          token_array_begin:
          case TOKEN_GROUP_BEGIN:
          case TOKEN_BLOCK_BEGIN: {
            REBARR *a = Scan_Child_Array(
                level, (token >= TOKEN_GET_BLOCK_BEGIN) ? ']' : ')'
            );

            enum Reb_Kind kind = KIND_OF_ARRAY_FROM_TOKEN(token);
            if (
                *ss->end == ':'  // `...(foo):` or `...[bar]:`
                and level->mode_char != '/'  // leave `:` to make SET-PATH!
            ){
                if (
                    token == TOKEN_GET_BLOCK_BEGIN
                    or token == TOKEN_GET_GROUP_BEGIN
                ){
                    goto syntax_error;  // `:(foo):` or `:[bar]:`
                }
                Init_Any_Array(DS_PUSH(), SETIFY_ANY_PLAIN_KIND(kind), a);
                ++ss->begin;
                ++ss->end;
            }
            else
                Init_Any_Array(DS_PUSH(), kind, a);
            ep = ss->end;
            break; }

        case TOKEN_PATH:
            if (*ss->end == '\0' or IS_LEX_SPACE(*ss->end)) {
                //
                // This means you have something like `/`, `foo//`, `///`...
                // Basically you don't expect to see a TOKEN_PATH while doing
                // a path scan unless you wind up at the end.
                //
                if (level->mode_char == '/') {
                    Init_Blank(DS_PUSH());
                    Init_Blank(DS_PUSH());
                    goto loop;
                }

                // Handle the simple `/` case

                REBARR *a = Make_Array(2);
                Init_Blank(ARR_AT(a, 0));
                Init_Blank(ARR_AT(a, 1));
                TERM_ARRAY_LEN(a, 2);
                Init_Path(DS_PUSH(), a);
                break;
            }

            Init_Blank(DS_PUSH());  // implicitly imagine blank per slash

            if (*ss->begin == '\0' or IS_LEX_SPACE(*ss->begin)) {
                Init_Blank(DS_PUSH());
                break;
            }

            if (level->mode_char != '/')  // saw slash(es), not scanning path
                goto scan_path_head_is_DS_TOP;

            goto loop;  // otherwise, we were scanning a path already

          case TOKEN_BLOCK_END: {
            if (level->mode_char == ']')
                goto array_done;

            if (level->mode_char == '/') {  // implicit end, such as `[lit /]`
                Init_Blank(DS_PUSH());
                --ss->begin;
                --ss->end;
                goto array_done;
            }

            if (level->mode_char != '\0')  // expected e.g. `)` before the `]`
                fail (Error_Mismatch(level, level->mode_char, ']'));

            // just a stray unexpected ']'
            //
            fail (Error_Extra(ss, ']')); }

          case TOKEN_GROUP_END: {
            if (level->mode_char == ')')
                goto array_done;

            if (level->mode_char == '/') {  // implicit end, such as `(lit /)`
                Init_Blank(DS_PUSH());
                --ss->begin;
                --ss->end;
                goto array_done;
            }

            if (level->mode_char != '\0')  // expected e.g. ']' before the ')'
                fail (Error_Mismatch(level, level->mode_char, ')'));

            // just a stray unexpected ')'
            //
            fail (Error_Extra(ss, ')')); }

          case TOKEN_INTEGER:  // INTEGER! or start of DATE!
            if (*ep != '/' or level->mode_char == '/') {
                if (ep != Scan_Integer(DS_PUSH(), bp, len))
                    goto syntax_error;
            }
            else {  // saw a `/` not in block
                token = TOKEN_DATE;
                while (*ep == '/' or IS_LEX_NOT_DELIMIT(*ep))
                    ++ep;
                len = cast(REBLEN, ep - bp);
                if (ep != Scan_Date(DS_PUSH(), bp, len))
                    goto syntax_error;

                // !!! used to just set ss->begin to ep...which tripped up an
                // assert that ss->end is greater than ss->begin at the start
                // of the loop.  So this sets both to ep.  Review.

                ss->begin = ss->end = ep;
            }
            break;

          case TOKEN_DECIMAL:
          case TOKEN_PERCENT:
            if (*ep == '/')
                goto syntax_error;  // Do not allow 1.2/abc

            if (ep != Scan_Decimal(DS_PUSH(), bp, len, false))
                goto syntax_error;

            if (bp[len - 1] == '%') {
                RESET_VAL_HEADER(DS_TOP, REB_PERCENT, CELL_MASK_NONE);
                VAL_DECIMAL(DS_TOP) /= 100.0;
            }
            break;

          case TOKEN_MONEY:
            if (*ep == '/') {  // Do not allow $1/$2
                ++ep;
                goto syntax_error;
            }
            if (ep != Scan_Money(DS_PUSH(), bp, len))
                goto syntax_error;
            break;

          case TOKEN_TIME:
            if (
                bp[len - 1] == ':'
                and level->mode_char == '/'  // could be path/10: set
            ){
                if (ep - 1 != Scan_Integer(DS_PUSH(), bp, len - 1))
                    goto syntax_error;
                ss->end--;  // put ':' back on end but not beginning
                break;
            }
            if (ep != Scan_Time(DS_PUSH(), bp, len))
                goto syntax_error;
            break;

          case TOKEN_DATE:
            while (*ep == '/' and level->mode_char != '/') {  // Is date/time?
                ep++;
                while (IS_LEX_NOT_DELIMIT(*ep)) ep++;
                len = cast(REBLEN, ep - bp);
                if (len > 50) {
                    // prevent infinite loop, should never be longer than this
                    break;
                }
                ss->begin = ep;  // End point extended to cover time
            }
            if (ep != Scan_Date(DS_PUSH(), bp, len))
                goto syntax_error;
            break;

          case TOKEN_CHAR: {
            REBUNI uni;
            bp += 2;  // skip #", and subtract 1 from ep for "
            if (ep - 1 != Scan_UTF8_Char_Escapable(&uni, bp))
                goto syntax_error;
            Init_Char_May_Fail(DS_PUSH(), uni);
            break; }

          case TOKEN_STRING:  // UTF-8 pre-scanned above, and put in MOLD_BUF
            Init_Text(DS_PUSH(), Pop_Molded_String(mo));
            break;

          case TOKEN_BINARY:
            if (ep != Scan_Binary(DS_PUSH(), bp, len))
                goto syntax_error;
            break;

          case TOKEN_PAIR:
            if (ep != Scan_Pair(DS_PUSH(), bp, len))
                goto syntax_error;
            break;

          case TOKEN_TUPLE:
            if (ep != Scan_Tuple(DS_PUSH(), bp, len))
                goto syntax_error;
            break;

          case TOKEN_FILE:
            if (ep != Scan_File(DS_PUSH(), bp, len))
                goto syntax_error;
            break;

          case TOKEN_EMAIL:
            if (ep != Scan_Email(DS_PUSH(), bp, len))
                goto syntax_error;
            break;

          case TOKEN_URL:
            if (ep != Scan_URL(DS_PUSH(), bp, len))
                goto syntax_error;
            break;

          case TOKEN_TAG:
            //
            // The Scan_Any routine (only used here for tag) doesn't
            // know where the tag ends, so it scans the len.
            //
            if (ep - 1 != Scan_Any(
                DS_PUSH(),
                bp + 1,
                len - 2,
                REB_TAG,
                STRMODE_NO_CR
            )){
                goto syntax_error;
            }
            break;

          case TOKEN_CONSTRUCT: {
            REBARR *array = Scan_Child_Array(level, ']');

            // !!! Should the scanner be doing binding at all, and if so why
            // just Lib_Context?  Not binding would break functions entirely,
            // but they can't round-trip anyway.  See #2262.
            //
            Bind_Values_All_Deep(ARR_HEAD(array), Lib_Context);

            if (ARR_LEN(array) == 0 or not IS_WORD(ARR_HEAD(array))) {
                DECLARE_LOCAL (temp);
                Init_Block(temp, array);
                fail (Error_Malconstruct_Raw(temp));
            }

            REBSYM sym = VAL_WORD_SYM(ARR_HEAD(array));
            if (
                IS_KIND_SYM(sym)
                or sym == SYM_IMAGE_X
            ){
                if (ARR_LEN(array) != 2) {
                    DECLARE_LOCAL (temp);
                    Init_Block(temp, array);
                    fail (Error_Malconstruct_Raw(temp));
                }

                // !!! Having an "extensible scanner" is something that has
                // not been designed.  So the syntax `#[image! [...]]` for
                // loading images doesn't have a strategy now that image is
                // not baked in.  It adds to the concerns the scanner already
                // has about evaluation, etc.  However, there are tests based
                // on this...so we keep them loading and working for now.
                //
                enum Reb_Kind kind;
                MAKE_HOOK *hook;
                if (sym == SYM_IMAGE_X) {
                    kind = REB_CUSTOM;
                    hook = Make_Hook_For_Image();
                }
                else {
                    kind = KIND_FROM_SYM(sym);
                    hook = Make_Hook_For_Kind(kind);
                }

                // !!! As written today, MAKE may call into the evaluator, and
                // hence a GC may be triggered.  Performing evaluations during
                // the scanner is a questionable idea, but at the very least
                // `array` must be guarded, and a data stack cell can't be
                // used as the destination...because a raw pointer into the
                // data stack could go bad on any DS_PUSH() or DS_DROP().
                //
                DECLARE_LOCAL (cell);
                Init_Unreadable_Blank(cell);
                PUSH_GC_GUARD(cell);

                PUSH_GC_GUARD(array);
                const REBVAL *r = hook(
                    cell,
                    kind,
                    nullptr,
                    KNOWN(ARR_AT(array, 1))
                );
                if (r == R_THROWN) {  // !!! good argument for not using MAKE
                    assert(false);
                    fail ("MAKE during construction syntax threw--illegal");
                }
                if (r != cell) {  // !!! not yet supported
                    assert(false);
                    fail ("MAKE during construction syntax not out cell");
                }
                DROP_GC_GUARD(array);

                Move_Value(DS_PUSH(), cell);
                DROP_GC_GUARD(cell);
            }
            else {
                if (ARR_LEN(array) != 1) {
                    DECLARE_LOCAL (temp);
                    Init_Block(temp, array);
                    fail (Error_Malconstruct_Raw(temp));
                }

                // !!! Construction syntax allows the "type" slot to be one of
                // the literals #[false], #[true]... along with legacy #[none]
                // while the legacy #[unset] is no longer possible (but
                // could load some kind of erroring function value)
                //
                switch (sym) {
                  case SYM_NONE:  // !!! Should be under a LEGACY flag...
                    Init_Blank(DS_PUSH());
                    break;

                  case SYM_FALSE:
                    Init_False(DS_PUSH());
                    break;

                  case SYM_TRUE:
                    Init_True(DS_PUSH());
                    break;

                  case SYM_UNSET:  // !!! Should be under a LEGACY flag
                  case SYM_VOID:
                    Init_Void(DS_PUSH());
                    break;

                  default: {
                    DECLARE_LOCAL (temp);
                    Init_Block(temp, array);
                    fail (Error_Malconstruct_Raw(temp)); }
                }
            }
            break; }  // case TOKEN_CONSTRUCT

          case TOKEN_END:
            continue;

          default:
            panic ("Invalid TOKEN in Scanner.");
        }

        // !!! If there is a binder in effect, we also bind the item while
        // we have loaded it.  For now, assume any negative numbers are into
        // the lib context (which we do not expand) and any positive numbers
        // are into the user context (which we will expand).
        //
        if (ss->feed and ss->feed->binder and ANY_WORD(DS_TOP)) {
            REBSTR *canon = VAL_WORD_CANON(DS_TOP);
            REBINT n = Get_Binder_Index_Else_0(ss->feed->binder, canon);
            if (n > 0) {
                //
                // Exists in user context at the given positive index.
                //
                INIT_BINDING(DS_TOP, ss->feed->context);
                INIT_WORD_INDEX(DS_TOP, n);
            }
            else if (n < 0) {
                //
                // Index is the negative of where the value exists in lib.
                // A proxy needs to be imported from lib to context.
                //
                Expand_Context(ss->feed->context, 1);
                Move_Var(  // preserve enfix state
                    Append_Context(ss->feed->context, DS_TOP, 0),
                    CTX_VAR(ss->feed->lib, -n)  // -n is positive
                );
                REBINT check = Remove_Binder_Index_Else_0(
                    ss->feed->binder,
                    canon
                );
                assert(check == n);  // n is negative
                UNUSED(check);
                Add_Binder_Index(
                    ss->feed->binder,
                    canon,
                    VAL_WORD_INDEX(DS_TOP)
                );
            }
            else {
                // Doesn't exist in either lib or user, create a new binding
                // in user (this is not the preferred behavior for modules
                // and isolation, but going with it for the API for now).
                //
                Expand_Context(ss->feed->context, 1);
                Append_Context(ss->feed->context, DS_TOP, 0);
                Add_Binder_Index(
                    ss->feed->binder,
                    canon,
                    VAL_WORD_INDEX(DS_TOP)
                );
            }
        }

        if (level->mode_char == '/') {
            if (*ep != '/')  // e.g. `a/b`, just finished scanning b
                goto array_done;

            ++ep;

            if (
                *ep == '\0' or IS_LEX_SPACE(*ep) or ANY_CR_LF_END(*ep)
                or *ep == ')' or *ep == ']'
            ){  // e.g. `/a/`
                Init_Blank(DS_PUSH());  // `/a/` is path form of [_ a _]
                ss->begin = ep;
                goto array_done;
            }

            if (*ep == '/') {
                ss->begin = ep;
                goto loop;
            }

            if (*ep != '(' and *ep != '[' and IS_LEX_DELIMIT(*ep)) {
                token = TOKEN_PATH;  // so error says "bad path"
                goto syntax_error;
            }
            ss->begin = ep;  // skip next /
        }
        else if (*ep == '/') {
            //
            // We're noticing a path was actually starting with the token
            // that just got pushed, so it should be a part of that path.

            ++ss->begin;

          scan_path_head_is_DS_TOP: ;  // precedes declaration, need `;`

            REBDSP dsp_path_head = DSP;

            if (
                *ss->begin == '\0'  // `foo/`
                or IS_LEX_ANY_SPACE(*ss->begin)  // `foo/ bar`
                or *ss->begin == ';'  // `foo/;bar`
            ){
                // Don't bother scanning recursively if we don't have to.
                // Note we still might come up empty (e.g. `foo/)`)
            }
            else {
                SCAN_LEVEL child;
                child.ss = ss;
                child.start_line = level->start_line;
                child.start_line_head = level->start_line_head;
                child.opts = level->opts;
                child.mode_char = '/';
                child.newline_pending = false;

                Scan_To_Stack(&child);
            }

            // Any trailing colons should have been left on, because the child
            // scan noticed the mode_char was '/' and that we'd want it for
            // a SET-PATH!.  But if there was a leading colon, it got absorbed
            // into the first element of the array.  We need to account for
            // this by mutating any first element that's a GET-XXX! into a
            // plain XXX! and make this a GET-PATH!, and also check for
            // conflicts if there's a colon at the end and making a SET-PATH!

            if (DSP - dsp_path_head == 0) {  // nothing more added
                //
                // !!! Currently there is no special case optimization for
                // leading paths with a tail blank.  It could perhaps be
                // done by cutting out the allowance of escaping levels as
                // meaning for the kind byte.  Not a priority.
                //
                REBARR *a = Make_Array_Core(2, NODE_FLAG_MANAGED);
                MISC(a).line = ss->line;
                LINK_FILE_NODE(a) = NOD(ss->file);
                SET_ARRAY_FLAG(a, HAS_FILE_LINE_UNMASKED);
                SET_SERIES_FLAG(a, LINK_NODE_NEEDS_MARK);

                Append_Value(a, DS_TOP);  // may be BLANK!
                Init_Blank(Alloc_Tail_Array(a));
                if (GET_CELL_FLAG(DS_TOP, BLANK_MARKED_GET))
                    Init_Any_Path(DS_TOP, REB_GET_PATH, a);
                else
                    Init_Path(DS_TOP, a);
            }
            else if (
                DSP - dsp_path_head == 1  // one more item added
                and IS_BLANK(DS_AT(dsp_path_head))
                and NOT_CELL_FLAG(DS_AT(dsp_path_head), BLANK_MARKED_GET)
            ){
                // This is the optimized case where we use a single cell to
                // represent a path with a blank at the head like /FOO.  So
                // move the one value we scanned into the position we want
                // and apply the optimization.

                Refinify(Move_Value(DS_TOP - 1, DS_TOP));
                DS_DROP();
            }
            else {
                REBFLGS flags = NODE_FLAG_MANAGED;
                if (level->newline_pending)
                    flags |= ARRAY_FLAG_NEWLINE_AT_TAIL;

                bool blank_marked_get =
                    GET_CELL_FLAG(DS_AT(dsp_path_head), BLANK_MARKED_GET);
                if (blank_marked_get)
                    assert(IS_BLANK(DS_AT(dsp_path_head)));

                REBARR *a = Pop_Stack_Values_Core(
                    dsp_path_head - 1,  // stop popping right after head pop
                    flags
                );
                MISC(a).line = ss->line;
                LINK_FILE_NODE(a) = NOD(ss->file);
                SET_ARRAY_FLAG(a, HAS_FILE_LINE_UNMASKED);
                SET_SERIES_FLAG(a, LINK_NODE_NEEDS_MARK);

                DS_PUSH();

                RELVAL *head = ARR_HEAD(a);
                REBYTE kind_head = KIND_BYTE(head);

                if (ANY_GET_KIND(kind_head) or blank_marked_get) {
                    if (ss->begin and *ss->end == ':')
                      goto syntax_error;  // for instance `:a/b/c:`

                    RESET_VAL_HEADER(
                        DS_TOP,
                        REB_GET_PATH,
                        CELL_FLAG_FIRST_IS_NODE
                    );
                    if (not blank_marked_get)  // undecorated
                        mutable_KIND_BYTE(head)
                            = mutable_MIRROR_BYTE(head)
                            = UNGETIFY_ANY_GET_KIND(kind_head);
                }
                else if (ANY_SYM_KIND(kind_head)) {  // !!! TBD: blank hack
                    if (ss->begin and *ss->end == ':')
                      goto syntax_error;  // for instance `@a/b/c:`

                    RESET_VAL_HEADER(
                        DS_TOP,
                        REB_SYM_PATH,
                        CELL_FLAG_FIRST_IS_NODE
                    );
                    mutable_KIND_BYTE(head)
                        = mutable_MIRROR_BYTE(head)
                        = UNSYMIFY_ANY_SYM_KIND(kind_head);
                }
                else if (ss->begin and *ss->end == ':') {
                    RESET_VAL_HEADER(
                        DS_TOP,
                        REB_SET_PATH,
                        CELL_FLAG_FIRST_IS_NODE
                    );
                    ss->begin = ++ss->end;
                }
                else
                    RESET_VAL_HEADER(
                        DS_TOP,
                        REB_PATH,
                        CELL_FLAG_FIRST_IS_NODE
                    );

                INIT_VAL_NODE(DS_TOP, a);
                VAL_INDEX(DS_TOP) = 0;
                INIT_BINDING(DS_TOP, UNBOUND);
            }

            token = TOKEN_PATH;  // for error message !!! unused?
        }

        if (lit_depth != 0) {
            //
            // Transform the topmost value on the stack into a QUOTED!, to
            // account for the ''' that was preceding it.
            //
            Quotify(DS_TOP, lit_depth);
            lit_depth = 0;
        }

        // Set the newline on the new value, indicating molding should put a
        // line break *before* this value (needs to be done after recursion to
        // process paths or other arrays...because the newline belongs on the
        // whole array...not the first element of it).
        //
        if (level->newline_pending) {
            level->newline_pending = false;
            SET_CELL_FLAG(DS_TOP, NEWLINE_BEFORE);
        }

        // Added for TRANSCODE/NEXT (LOAD/NEXT is deprecated, see #1703)
        //
        if (just_once)
            goto array_done;
    }

    // At some point, a token for an end of block or group needed to jump to
    // the array_done.  If it didn't, we never got a proper closing.
    //
    if (level->mode_char == ']' or level->mode_char == ')')
        fail (Error_Missing(level, level->mode_char));

  array_done:

    Drop_Mold_If_Pushed(mo);

    if (lit_depth != 0)
        Quotify(Init_Nulled(DS_PUSH()), lit_depth);

    // Note: ss->newline_pending may be true; used for ARRAY_NEWLINE_AT_TAIL

    return nullptr;  // to be used w/rebRescue(), has to return a REBVAL*

  syntax_error:

    fail (Error_Syntax(ss, token));
}



//
//  Scan_To_Stack_Relaxed_Failed: C
//
// If the scan failed, the error will be on the top of the stack.  (This is
// done to avoid passing in a potentially volatile memory location, e.g.
// the result of getting a variable location.)
//
bool Scan_To_Stack_Relaxed_Failed(SCAN_LEVEL *level) {
    SCAN_STATE *ss = level->ss;
    SCAN_LEVEL before = *level;
    SCAN_STATE ss_before = *level->ss;

    REBVAL *error = rebRescue(cast(REBDNG*, &Scan_To_Stack), level);
    if (not error)
        return false;  // scan went fine, hopefully the common case...

    // !!! See notes on ->depth regarding TRANSCODE/RELAX and the problems
    // with trying to do recoverable transcoding.  It was a half-baked feature
    // in R3-Alpha that we try to keep in some form, but we only attempt to
    // actually recover the parse if we're not in a nested block.  Otherwise
    // we fail since you cannot recover without more scanner state exposure.
    //
    if (ss->depth != 0)
        fail (VAL_CONTEXT(error));

    before.ss = &ss_before;

    // Because rebRescue() restores the data stack, the in-progress scan
    // contents were lost.  But the `ss` state tells us where the token was
    // that caused the problem.  Assuming a deterministic scanner, we can
    // re-run the process...just stopping before the bad token.  Assuming
    // errors aren't rampant, this is likely more efficient than rebRescue()
    // on each individual token parse, and less invasive than trying to come
    // up with a form of rescueing that leaves the data stack as-is.
    //
    if (ss->begin == ss_before.begin) {
        //
        // Couldn't consume *any* UTF-8 input...so don't bother re-running.
    }
    else {
        // !!! The ss->limit feature was not implemented in R3-Alpha, it would
        // stop on `\0` only.  May have immutable const data, so poking a `\0`
        // into it may be unsafe.  Make a copy of the UTF-8 input that managed
        // to get consumed, terminate it, and use that.  Hope errors are rare,
        // and if this becomes a problem, implement ss->limit.
        //
        REBLEN limit = ss->begin - ss_before.begin;
        REBSER *bin = Make_Binary(limit);
        memcpy(BIN_HEAD(bin), ss_before.begin, limit);
        TERM_BIN_LEN(bin, limit);

        SET_SERIES_FLAG(bin, DONT_RELOCATE);  // BIN_HEAD() is cached
        ss_before.begin = BIN_HEAD(bin);
        TRASH_POINTER_IF_DEBUG(ss_before.end);

        Scan_To_Stack(&before);  // !!! Shouldn't error...check that?

        Free_Unmanaged_Series(bin);
    }

    ss->begin = ss->end;  // skip malformed token

    Move_Value(DS_PUSH(), error);
    rebRelease(error);
    return true;
}


//
//  Scan_Child_Array: C
//
// This routine would create a new structure on the scanning stack.  Putting
// what would be local variables for each level into a structure helps with
// reflection, allowing for better introspection and error messages.  (This
// is similar to the benefits of Reb_Frame.)
//
static REBARR *Scan_Child_Array(SCAN_LEVEL *parent, REBYTE mode_char)
{
    assert(mode_char == ')' or mode_char == ']');

    SCAN_STATE *ss = parent->ss;
    ++ss->depth;

    SCAN_LEVEL child;
    child.ss = ss;

    // Capture current line and head of line into the starting points, because
    // some errors wish to report the start of the array's location.
    //
    child.start_line = ss->line;
    child.start_line_head = ss->line_head;
    child.newline_pending = false;
    child.opts = parent->opts &= ~(SCAN_FLAG_NULLEDS_LEGAL | SCAN_FLAG_NEXT);

    REBDSP dsp_orig = DSP;

    child.mode_char = mode_char;
    Scan_To_Stack(&child);

    REBARR *a = Pop_Stack_Values_Core(
        dsp_orig,
        NODE_FLAG_MANAGED
            | (child.newline_pending ? ARRAY_FLAG_NEWLINE_AT_TAIL : 0)
    );

    // Tag array with line where the beginning bracket/group/etc. was found
    //
    MISC(a).line = ss->line;
    LINK_FILE_NODE(a) = NOD(ss->file);
    SET_ARRAY_FLAG(a, HAS_FILE_LINE_UNMASKED);
    SET_SERIES_FLAG(a, LINK_NODE_NEEDS_MARK);

    --ss->depth;
    return a;
}


//
//  Scan_UTF8_Managed: C
//
// Scan source code. Scan state initialized. No header required.
//
REBARR *Scan_UTF8_Managed(REBSTR *filename, const REBYTE *utf8, REBSIZ size)
{
    SCAN_STATE ss;
    SCAN_LEVEL level;
    const REBLIN start_line = 1;
    Init_Scan_Level(&level, &ss, filename, start_line, utf8, size);

    REBDSP dsp_orig = DSP;
    Scan_To_Stack(&level);

    REBARR *a = Pop_Stack_Values_Core(
        dsp_orig,
        NODE_FLAG_MANAGED
            | (level.newline_pending ? ARRAY_FLAG_NEWLINE_AT_TAIL : 0)
    );

    MISC(a).line = ss.line;
    LINK_FILE_NODE(a) = NOD(ss.file);
    SET_ARRAY_FLAG(a, HAS_FILE_LINE_UNMASKED);
    SET_SERIES_FLAG(a, LINK_NODE_NEEDS_MARK);

    return a;
}


//
//  Scan_Header: C
//
// Scan for header, return its offset if found or -1 if not.
//
REBINT Scan_Header(const REBYTE *utf8, REBLEN len)
{
    SCAN_LEVEL level;
    SCAN_STATE ss;
    REBSTR * const filename = Canon(SYM___ANONYMOUS__);
    const REBLIN start_line = 1;
    Init_Scan_Level(&level, &ss, filename, start_line, utf8, len);

    REBINT result = Scan_Head(&ss);
    if (result == 0)
        return -1;

    const REBYTE *cp = ss.begin - 2;

    // Backup to start of header

    if (result > 0) {  // normal header found
        while (cp != utf8 and *cp != 'r' and *cp != 'R')
            --cp;
    } else {
        while (cp != utf8 and *cp != '[')
            --cp;
    }
    return cast(REBINT, cp - utf8);
}


//
//  Startup_Scanner: C
//
void Startup_Scanner(void)
{
    REBLEN n = 0;
    while (Token_Names[n])
        ++n;
    assert(cast(enum Reb_Token, n) == TOKEN_MAX);
}


//
//  Shutdown_Scanner: C
//
void Shutdown_Scanner(void)
{
}


//
//  transcode: native [
//
//  {Translates UTF-8 source (from a text or binary) to values}
//
//      return: "Transcoded value (or block of values)"
//          [<opt> any-value!]
//      source "If BINARY!, must be Unicode UTF-8 encoded"
//          [text! binary!]
//      /next "Translate next complete value and give back next position"
//          [<output>]  ; <opt> text! binary!
//      /relax "Return an error and skip token if possible (top level only)"
//          [<output>]  ; <opt> error!
//      /file "File to be associated with BLOCK!s and GROUP!s in source"
//          [file! url!]
//      /line "Line number for start of scan, word variable will be updated"
//          [integer! any-word!]
//  ]
//
REBNATIVE(transcode)
//
// R3-Alpha's TRANSCODE would return a length 2 BLOCK!.  Ren-C uses multiple
// return values, and operates in a reduced case where if you ask for only
// one return value then it assumes you want the entire thing transcoded...
// but if you ask for 2 it assumes you want partial and 3 assumes you would
// like errors reported as a value instead of needing a TRAP.
//
// https://github.com/rebol/rebol-issues/issues/1916
//
// !!! See notes on SCAN_STATE->depth for explanations of why the /RELAX
// option works only at the top level.  The /ONLY option was removed entirely,
// as it was fairly useless...taking items out of blocks but then would wind
// up failing when it hit the closing brace on successive calls.  A more
// coherent notion of continuable scanner state is required.
{
    INCLUDE_PARAMS_OF_TRANSCODE;

    REBVAL *source = ARG(source);

    // !!! Should the base name and extension be stored, or whole path?
    //
    REBSTR *filename = REF(file)
        ? Intern(ARG(file))
        : Canon(SYM___ANONYMOUS__);

    const REBVAL *line_number;
    if (ANY_WORD(ARG(line)))
        line_number = Lookup_Word_May_Fail(ARG(line), SPECIFIED);
    else
        line_number = ARG(line);

    REBLIN start_line;
    if (IS_NULLED(line_number)) {
        start_line = 1;
    }
    else if (IS_INTEGER(line_number)) {
        start_line = VAL_INT32(line_number);
        if (start_line <= 0)
            fail (PAR(line));
    }
    else
        fail ("/LINE must be an INTEGER! or an ANY-WORD! integer variable");

    REBSIZ size;
    const REBYTE *bp = VAL_BYTES_AT(&size, source);

    SCAN_LEVEL level;
    SCAN_STATE ss;
    Init_Scan_Level(&level, &ss, filename, start_line, bp, size);

    if (REF(next))
        level.opts |= SCAN_FLAG_NEXT;

    // If the source data bytes are "1" then the scanner will push INTEGER! 1
    // if the source data is "[1]" then the scanner will push BLOCK! [1]
    //
    // Return a block of the results, so [1] and [[1]] in those cases.
    //
    REBDSP dsp_orig = DSP;
    if (REF(relax)) {
        bool failed = Scan_To_Stack_Relaxed_Failed(&level);

        REBVAL *var = Lookup_Mutable_Word_May_Fail(ARG(relax), SPECIFIED);
        if (failed) {
            Move_Value(var, DS_TOP);
            DS_DROP();
        }
        else
            Init_Nulled(var);
    }
    else
        Scan_To_Stack(&level);

    if (REF(next)) {
        if (DSP == dsp_orig)
            Init_Nulled(D_OUT);
        else {
            Move_Value(D_OUT, DS_TOP);
            DS_DROP();
        }
        assert(DSP == dsp_orig);
    }
    else {
        REBARR *a = Pop_Stack_Values_Core(
            dsp_orig,
            NODE_FLAG_MANAGED
                | (level.newline_pending ? ARRAY_FLAG_NEWLINE_AT_TAIL : 0)
        );
        MISC(a).line = ss.line;
        LINK_FILE_NODE(a) = NOD(ss.file);
        SER(a)->header.bits |= ARRAY_MASK_HAS_FILE_LINE;

        Init_Block(D_OUT, a);
    }

    if (ANY_WORD(ARG(line)))  // they wanted the line number updated
        Init_Integer(Sink_Word_May_Fail(ARG(line), SPECIFIED), ss.line);

    // Return the input BINARY! or TEXT! advanced by how much the transcode
    // operation consumed.
    //
    if (REF(next)) {
        REBVAL *var = Sink_Word_May_Fail(ARG(next), SPECIFIED);
        Move_Value(var, source);

        if (IS_BINARY(var))
            VAL_INDEX(var) = ss.end - VAL_BIN_HEAD(var);
        else {
            assert(IS_TEXT(var));

            // !!! The scanner does not currently keep track of how many
            // codepoints it went past, it only advances bytes.  But the TEXT!
            // we're returning here needs a codepoint-based index.
            //
            // Count characters by going backwards from the byte position of
            // the finished scan until the byte we started at is found.
            //
            // (It would probably be better if the scanner kept count, though
            // maybe that would make it slower when this isn't needed?)
            //
            if (ss.begin != 0)
                VAL_INDEX(var) += Num_Codepoints_For_Bytes(bp, ss.begin);
            else
                VAL_INDEX(var) += BIN_TAIL(VAL_SERIES(var)) - bp;
        }
    }

    return D_OUT;
}


//
//  Scan_Any_Word: C
//
// Scan word chars and make word symbol for it.
// This method gets exactly the same results as scanner.
// Returns symbol number, or zero for errors.
//
const REBYTE *Scan_Any_Word(
    REBVAL *out,
    enum Reb_Kind kind,
    const REBYTE *utf8,
    REBLEN len
) {
    SCAN_LEVEL level;
    SCAN_STATE ss;
    REBSTR * const filename = Canon(SYM___ANONYMOUS__);
    const REBLIN start_line = 1;
    Init_Scan_Level(&level, &ss, filename, start_line, utf8, len);

    DECLARE_MOLD (mo);

    enum Reb_Token token = Locate_Token_May_Push_Mold(mo, &level);
    if (token != TOKEN_WORD)
        return nullptr;

    Init_Any_Word(out, kind, Intern_UTF8_Managed(utf8, len));
    Drop_Mold_If_Pushed(mo);
    return ss.begin;
}


//
//  Scan_Issue: C
//
// Scan an issue word, allowing special characters.
// Returning null should trigger an error in the caller.
//
const REBYTE *Scan_Issue(RELVAL *out, const REBYTE *cp, REBLEN len)
{
    if (len == 0)
        return nullptr;

    while (IS_LEX_SPACE(*cp))  // skip whitespace
        ++cp;

    const REBYTE *bp = cp;

    REBLEN l = len;
    while (l > 0) {
        switch (GET_LEX_CLASS(*bp)) {
          case LEX_CLASS_DELIMIT:
            return nullptr;

          case LEX_CLASS_SPECIAL: {  // Flag all but first special char
            REBLEN c = GET_LEX_VALUE(*bp);
            if (
                LEX_SPECIAL_APOSTROPHE != c
                and LEX_SPECIAL_COMMA != c
                and LEX_SPECIAL_PERIOD != c
                and LEX_SPECIAL_PLUS != c
                and LEX_SPECIAL_MINUS != c
                and LEX_SPECIAL_BAR != c
                and LEX_SPECIAL_BLANK != c
                and LEX_SPECIAL_COLON != c

                // !!! R3-Alpha didn't allow #<< or #>>, but this was used
                // in things like pdf-maker.r - and Red allows it.  Ren-C
                // aims to make ISSUE!s back into strings, so allow it here.
                //
                and LEX_SPECIAL_GREATER != c
                and LEX_SPECIAL_LESSER != c
            ){
                return nullptr;
            }
            goto lex_word_or_number; }

          lex_word_or_number:
          case LEX_CLASS_WORD:
          case LEX_CLASS_NUMBER:
            ++bp;
            --l;
            break;
        }
    }

    Init_Issue(out, Make_Sized_String_UTF8(cs_cast(cp), len));
    return bp;
}
