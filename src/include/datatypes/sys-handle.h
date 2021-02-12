//
//  File: %sys-handle.h
//  Summary: "Definitions for GC-able and non-GC-able Handles"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012-2021 Ren-C Open Source Contributors
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Lesser GPL, Version 3.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.gnu.org/licenses/lgpl-3.0.html
//
//=////////////////////////////////////////////////////////////////////////=//
//
// In R3-Alpha, a HANDLE! was just a cell that would hold an arbitrary C
// data pointer.  The pointer was not shared as the cell was copied around...
// so it could not be changed and reflected in other instances.
//
// Ren-C kept that "cheap" form, but also added a variant "managed" form of
// HANDLE that keeps its data inside of a shared tracking node.  This means
// that operations can change the data and have the change reflected in other
// references to that handle.
//
// Another feature of the managed form is that the node can hold a hook for
// a "cleanup" function.  The GC will call this when there are no references
// left to the handle.
//
////=// NOTES /////////////////////////////////////////////////////////////=//
//
// * The C language spec says that data pointers and function pointers on a
//   platform may not be the same size.  Many codebases ignore this and
//   assume that they are, but HANDLE! tries to stay on the right side of
//   the spec and has different forms for functions and data.
//

#define INIT_VAL_HANDLE_SINGULAR        INIT_VAL_NODE1
#define VAL_HANDLE_SINGULAR(v)          ARR(VAL_NODE1(v))

#define VAL_HANDLE_LENGTH_U(v)          PAYLOAD(Any, (v)).second.u

#define VAL_HANDLE_CDATA_P(v)           EXTRA(Any, (v)).p
#define VAL_HANDLE_CFUNC_P(v)           EXTRA(Any, (v)).cfunc


inline static bool Is_Handle_Cfunc(REBCEL(const*) v) {
    assert(CELL_KIND(v) == REB_HANDLE);
    return VAL_HANDLE_LENGTH_U(v) == 0;
}

inline static REBCEL(const*) VAL_HANDLE_CANON(REBCEL(const*) v) {
    assert(CELL_KIND(v) == REB_HANDLE);
    if (NOT_CELL_FLAG(v, FIRST_IS_NODE))
        return v;  // changing handle instance won't be seen by copies
    return ARR_SINGLE(VAL_HANDLE_SINGULAR(v));  // has shared node
}

inline static RELVAL *mutable_VAL_HANDLE_CANON(RELVAL *v) {
    assert(IS_HANDLE(v));
    if (NOT_CELL_FLAG(v, FIRST_IS_NODE))
        return v;  // changing handle instance won't be seen by copies
    return ARR_SINGLE(VAL_HANDLE_SINGULAR(v));  // has shared node
}

inline static uintptr_t VAL_HANDLE_LEN(REBCEL(const*) v) {
    assert(not Is_Handle_Cfunc(v));
    return VAL_HANDLE_LENGTH_U(VAL_HANDLE_CANON(v));
}

inline static void *VAL_HANDLE_VOID_POINTER(REBCEL(const*) v) {
    assert(not Is_Handle_Cfunc(v));
    return VAL_HANDLE_CDATA_P(VAL_HANDLE_CANON(v));
}

#define VAL_HANDLE_POINTER(T, v) \
    cast(T*, VAL_HANDLE_VOID_POINTER(v))

inline static CFUNC *VAL_HANDLE_CFUNC(REBCEL(const*) v) {
    assert(Is_Handle_Cfunc(v));
    return VAL_HANDLE_CFUNC_P(VAL_HANDLE_CANON(v));
}

inline static CLEANUP_CFUNC *VAL_HANDLE_CLEANER(REBCEL(const*) v) {
    assert(CELL_KIND(v) == REB_HANDLE);
    if (NOT_CELL_FLAG(v, FIRST_IS_NODE))
        return nullptr;
    return VAL_HANDLE_SINGULAR(v)->misc.cleaner;
}

inline static void SET_HANDLE_LEN(RELVAL *v, uintptr_t length)
  { VAL_HANDLE_LENGTH_U(mutable_VAL_HANDLE_CANON(v)) = length; }

inline static void SET_HANDLE_CDATA(RELVAL *v, void *cdata) {
    RELVAL *canon = mutable_VAL_HANDLE_CANON(v);
    assert(VAL_HANDLE_LENGTH_U(canon) != 0);
    VAL_HANDLE_CDATA_P(canon) = cdata;
}

inline static void SET_HANDLE_CFUNC(RELVAL *v, CFUNC *cfunc) {
    assert(Is_Handle_Cfunc(v));
    RELVAL *canon = mutable_VAL_HANDLE_CANON(v);
    assert(VAL_HANDLE_LENGTH_U(canon) == 0);
    VAL_HANDLE_CFUNC_P(canon) = cfunc;
}

inline static REBVAL *Init_Handle_Cdata(
    RELVAL *out,
    void *cdata,
    uintptr_t length
){
    assert(length != 0);  // can't be 0 unless cfunc (see also malloc(0))
    RESET_CELL(out, REB_HANDLE, CELL_MASK_NONE);  // payload first is not node
    PAYLOAD(Any, out).first.trash = out;  // must initialize field
    VAL_HANDLE_CDATA_P(out) = cdata;
    VAL_HANDLE_LENGTH_U(out) = length;  // non-zero signals cdata
    return cast(REBVAL*, out);
}

inline static REBVAL *Init_Handle_Cfunc(
    RELVAL *out,
    CFUNC *cfunc
){
    RESET_CELL(out, REB_HANDLE, CELL_MASK_NONE);  // payload first is not node
    PAYLOAD(Any, out).first.trash = out;  // must initialize field
    VAL_HANDLE_CFUNC_P(out) = cfunc;
    VAL_HANDLE_LENGTH_U(out) = 0;  // signals cfunc
    return cast(REBVAL*, out);
}

inline static void Init_Handle_Managed_Common(
    RELVAL *out,
    uintptr_t length,
    CLEANUP_CFUNC *cleaner
){
    REBARR *singular = Alloc_Singular(FLAG_FLAVOR(HANDLE) | NODE_FLAG_MANAGED);
    singular->misc.cleaner = cleaner;

    RELVAL *single = ARR_SINGLE(singular);
    RESET_VAL_HEADER(single, REB_HANDLE, CELL_FLAG_FIRST_IS_NODE);
    INIT_VAL_HANDLE_SINGULAR(single, singular);
    VAL_HANDLE_LENGTH_U(single) = length;
    // caller fills in VAL_HANDLE_CDATA_P or VAL_HANDLE_CFUNC_P

    // Don't fill the handle properties in the instance if it's the managed
    // form.  This way, you can set the properties in the canon value and
    // effectively update all instances...since the bits live in the shared
    // series component.
    //
    RESET_CELL(out, REB_HANDLE, CELL_FLAG_FIRST_IS_NODE);
    INIT_VAL_HANDLE_SINGULAR(out, singular);
    VAL_HANDLE_LENGTH_U(out) = 0xDECAFBAD;  // trash to avoid compiler warning
    VAL_HANDLE_CDATA_P(out) = nullptr;  // or complains about not initializing
}

inline static REBVAL *Init_Handle_Cdata_Managed(
    RELVAL *out,
    void *cdata,
    uintptr_t length,
    CLEANUP_CFUNC *cleaner
){
    Init_Handle_Managed_Common(out, length, cleaner);

    // Leave the non-singular cfunc as trash; clients should not be using

    REBARR *a = VAL_HANDLE_SINGULAR(out);
    VAL_HANDLE_CDATA_P(ARR_SINGLE(a)) = cdata;
    return cast(REBVAL*, out);
}

inline static REBVAL *Init_Handle_Cdata_Managed_Cfunc(
    RELVAL *out,
    CFUNC *cfunc,
    CLEANUP_CFUNC *cleaner
){
    Init_Handle_Managed_Common(out, 0, cleaner);

    // Leave the non-singular cfunc as trash; clients should not be using
    
    REBARR *a = VAL_HANDLE_SINGULAR(out);
    VAL_HANDLE_CFUNC_P(ARR_SINGLE(a)) = cfunc;
    return cast(REBVAL*, out);
}
