//
//  File: %sys-rebpat.h
//  Summary: {Definitions for the Virtual Bind and Single Variable LET Node}
//  Project: "Ren-C Interpreter and Run-time"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2021 Ren-C Open Source Contributors
//
// See README.md and CREDITS.md for more information
//
// Licensed under the Lesser GPL, Version 3.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.gnu.org/licenses/lgpl-3.0.html
//
//=////////////////////////////////////////////////////////////////////////=//
//
// See %sys-patch.h for a description of virtual binding patches.
//
// There is currently not a separate REBPAT* type (it's just a REBARR) but
// there might need to be one for clarity, eventually.  This file defines the
// flags and layout because they're needed by inline functions before
// %sys-patch.h is included.
//


// The virtual binding patches keep a circularly linked list of their variants
// that have distinct next pointers.  This way, they can look through that
// list before creating an equivalent chain to one that already exists.
//
#define MISC_Variant_TYPE        REBARR*
#define MISC_Variant_CAST        ARR
#define HAS_MISC_Variant         FLAVOR_PATCH


//=//// PATCH_FLAG_REUSED /////////////////////////////////////////////////=//
//
// It's convenient to be able to know when a patch returned from a make call
// is reused or not.  But adding that parameter to the interface complicates
// it, and all not clients care.  There's plenty of bits free on patch array
// flags, so just use one.
//
#define PATCH_FLAG_REUSED \
    SERIES_FLAG_24


//=//// PATCH_FLAG_LET ////////////////////////////////////////////////////=//
//
// This signifies that a patch was made using LET, and hence it doesn't point
// to an object...rather the contents are the variable itself.  The LINK()
// holds the symbol.
//
#define PATCH_FLAG_LET \
    SERIES_FLAG_25



// The link slot for patches is available for use...
//
#define LINK_PatchSymbol_TYPE           const REBSYM*
#define LINK_PatchSymbol_CAST           SYM
#define HAS_LINK_PatchSymbol            FLAVOR_PATCH

#define INODE_NextPatch_TYPE            REBARR*
#define INODE_NextPatch_CAST            ARR
#define HAS_INODE_NextPatch             FLAVOR_PATCH

// Next node is either to another patch, a frame specifier REBCTX, or nullptr.
//

#define NextPatch(patch) \
    INODE(NextPatch, patch)
