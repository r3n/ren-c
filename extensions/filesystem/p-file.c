//
//  File: %p-file.c
//  Summary: "file port interface"
//  Section: ports
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Ren-C Open Source Contributors
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

#include "sys-core.h"

#include "file-req.h"

#define MAX_READ_MASK 0x7FFFFFFF  // max size per chunk

enum act_open_mask {
    AM_OPEN_NEW = 1 << 0,
    AM_OPEN_READ = 1 << 1,
    AM_OPEN_WRITE = 1 << 2,
    AM_OPEN_SEEK = 1 << 3,
    AM_OPEN_ALLOW = 1 << 4
};


//
//  Setup_File: C
//
// Convert native action refinements to file modes.
//
static void Setup_File(REBREQ *file, REBFLGS flags, REBVAL *path)
{
    struct rebol_devreq *req = Req(file);

    if (flags & AM_OPEN_WRITE)
        req->modes |= RFM_WRITE;
    if (flags & AM_OPEN_READ)
        req->modes |= RFM_READ;
    if (flags & AM_OPEN_SEEK)
        req->modes |= RFM_SEEK;

    if (flags & AM_OPEN_NEW) {
        req->modes |= RFM_NEW;
        if (not (flags & AM_OPEN_WRITE))
            fail (Error_Bad_File_Mode_Raw(path));
    }

    ReqFile(file)->path = path;

    Secure_Port(Canon(SYM_FILE), file, path /* , file->path */);

    // !!! For the moment, assume `path` has a lifetime that will exceed
    // the operation.  This will be easier to ensure once the REQ state is
    // Rebol-structured data, visible to the GC.
}


//
//  Cleanup_File: C
//
static void Cleanup_File(REBREQ *file)
{
    struct rebol_devreq *req = Req(file);
    req->flags &= ~RRF_OPEN;
}


//
//  Query_File_Or_Dir: C
//
// Produces a STD_FILE_INFO object.
//
REBVAL *Query_File_Or_Dir(const REBVAL *port, REBREQ *file)
{
    struct rebol_devreq *req = Req(file);
    assert(IS_FILE(ReqFile(file)->path));

    REBVAL *timestamp = File_Time_To_Rebol(file);

    return rebValue(
        "make ensure object! (", port , ")/scheme/info [",
            "name:", ReqFile(file)->path,
            "size:", rebI(ReqFile(file)->size),
            "type:", (req->modes & RFM_DIR) ? "'dir" : "'file",
            "date:", rebR(timestamp),
        "]",
        rebEND
    );
}


//
//  Open_File_Port: C
//
// Open a file port.
//
static void Open_File_Port(
    REBVAL *port,
    REBREQ *file,
    REBVAL *path
){
    UNUSED(port);

    struct rebol_devreq *req = Req(file);
    if (req->flags & RRF_OPEN)
        fail (Error_Already_Open_Raw(path));

    // Don't use OS_DO_DEVICE_SYNC() here, because we want to tack the file
    // name onto any error we get back.

    REBVAL *result = OS_DO_DEVICE(file, RDC_OPEN);
    assert(result != nullptr); // should be synchronous

    if (rebDid("error?", result, rebEND))
        fail (Error_Cannot_Open_Raw(ReqFile(file)->path, result));

    rebRelease(result);  // !!! ignore any other result?

    req->flags |= RRF_OPEN; // open it
}


REBINT Mode_Syms[] = {
    SYM_OWNER_READ,
    SYM_OWNER_WRITE,
    SYM_OWNER_EXECUTE,
    SYM_GROUP_READ,
    SYM_GROUP_WRITE,
    SYM_GROUP_EXECUTE,
    SYM_WORLD_READ,
    SYM_WORLD_WRITE,
    SYM_WORLD_EXECUTE,
    0
};


//
//  Read_File_Port: C
//
// Read from a file port.
//
static void Read_File_Port(
    REBVAL *out,
    REBVAL *port,
    REBREQ *file,
    REBVAL *path,
    REBFLGS flags,
    REBLEN len
) {
    assert(IS_FILE(path));

    UNUSED(path);
    UNUSED(flags);
    UNUSED(port);

    struct rebol_devreq *req = Req(file);

    REBSER *ser = Make_Binary(len); // read result buffer
    TERM_BIN_LEN(ser, len);
    Init_Binary(out, ser);

    // Do the read, check for errors:
    req->common.data = BIN_HEAD(ser);
    req->length = len;

    OS_DO_DEVICE_SYNC(file, RDC_READ);

    SET_SERIES_LEN(ser, req->actual);
    TERM_SEQUENCE(ser);
}


//
//  Write_File_Port: C
//
// !!! `len` comes from /PART, it should be in characters if a string and
// in bytes if a BINARY!.  It seems to disregard it if the data is BLOCK!
//
static void Write_File_Port(
    REBREQ *file,
    REBVAL *data,
    REBLEN limit,
    bool lines
){
    struct rebol_devreq *req = Req(file);

    if (IS_BLOCK(data)) {
        // Form the values of the block
        // !! Could be made more efficient if we broke the FORM
        // into 32K chunks for writing.
        DECLARE_MOLD (mo);
        Push_Mold(mo);
        if (lines)
            SET_MOLD_FLAG(mo, MOLD_FLAG_LINES);
        Form_Value(mo, data);
        Init_Text(data, Pop_Molded_String(mo)); // fall to next section
        limit = VAL_LEN_HEAD(data);
    }

    if (IS_TEXT(data)) {
        REBSIZ size;
        REBCHR(const*) utf8 = VAL_UTF8_LEN_SIZE_AT_LIMIT(
            nullptr,
            &size,
            data,
            limit
        );

        req->common.data = m_cast(REBYTE*,  // writing only
            cast(const REBYTE*, utf8)
        );
        req->length = size;
        req->modes |= RFM_TEXT; // do LF => CR LF, e.g. on Windows
    }
    else {
        req->common.data = m_cast(REBYTE*, VAL_BINARY_AT(data));
        req->length = limit;
        req->modes &= ~RFM_TEXT; // don't do LF => CR LF, e.g. on Windows
    }

    OS_DO_DEVICE_SYNC(file, RDC_WRITE);
}


//
//  Set_Length: C
//
// Note: converts 64bit number to 32bit. The requested size
// can never be greater than 4GB.  If limit isn't negative it
// constrains the size of the requested read.
//
static REBLEN Set_Length(REBREQ *file, REBI64 limit)
{
    // how much is already used
    REBI64 len = ReqFile(file)->size - ReqFile(file)->index;

    // Compute and bound bytes remaining:
    if (len < 0)
        return 0;
    len &= MAX_READ_MASK; // limit the size

    // Return requested length:
    if (limit < 0) return (REBLEN)len;

    // Limit size of requested read:
    if (limit > len) return cast(REBLEN, len);
    return cast(REBLEN, limit);
}


//
//  Set_Seek: C
//
// Computes the number of bytes that should be skipped.
//
static void Set_Seek(REBREQ *file, REBVAL *arg)
{
    struct rebol_devreq *req = Req(file);

    REBI64 i = Int64s(arg, 0);

    if (i > ReqFile(file)->size)
        i = ReqFile(file)->size;

    ReqFile(file)->index = i;

    req->modes |= RFM_RESEEK; // force a seek
}


//
//  File_Actor: C
//
// Internal port handler for files.
//
REB_R File_Actor(REBFRM *frame_, REBVAL *port, const REBVAL *verb)
{
    REBCTX *ctx = VAL_CONTEXT(port);
    REBVAL *spec = CTX_VAR(ctx, STD_PORT_SPEC);
    if (!IS_OBJECT(spec))
        fail (Error_Invalid_Spec_Raw(spec));

    REBVAL *path = Obj_Value(spec, STD_PORT_SPEC_HEAD_REF);
    if (path == NULL)
        fail (Error_Invalid_Spec_Raw(spec));

    if (IS_URL(path))
        path = Obj_Value(spec, STD_PORT_SPEC_HEAD_PATH);
    else if (!IS_FILE(path))
        fail (Error_Invalid_Spec_Raw(path));

    REBREQ *file = Force_Get_Port_State(port, &Dev_File);
    struct rebol_devreq *req = Req(file);

    // !!! R3-Alpha never implemented quite a number of operations on files,
    // including FLUSH, POKE, etc.

    switch (VAL_WORD_SYM(verb)) {
      case SYM_REFLECT: {
        INCLUDE_PARAMS_OF_REFLECT;

        UNUSED(ARG(value)); // implicitly comes from `port`
        REBSYM property = VAL_WORD_SYM(ARG(property));
        assert(property != SYM_0);

        switch (property) {
          case SYM_INDEX:
            return Init_Integer(D_OUT, ReqFile(file)->index + 1);

          case SYM_LENGTH:
            //
            // Comment said "clip at zero"
            ///
            return Init_Integer(D_OUT, ReqFile(file)->size - ReqFile(file)->index);

          case SYM_HEAD:
            ReqFile(file)->index = 0;
            req->modes |= RFM_RESEEK;
            RETURN (port);

          case SYM_TAIL:
            ReqFile(file)->index = ReqFile(file)->size;
            req->modes |= RFM_RESEEK;
            RETURN (port);

          case SYM_HEAD_Q:
            return Init_Logic(D_OUT, ReqFile(file)->index == 0);

          case SYM_TAIL_Q:
            return Init_Logic(D_OUT, ReqFile(file)->index >= ReqFile(file)->size);

          case SYM_PAST_Q:
            return Init_Logic(D_OUT, ReqFile(file)->index > ReqFile(file)->size);

        case SYM_OPEN_Q:
            return Init_Logic(D_OUT, did (req->flags & RRF_OPEN));

        default:
            break;
        }

        break; }

      case SYM_READ: {
        INCLUDE_PARAMS_OF_READ;

        UNUSED(PAR(source));
        UNUSED(PAR(string)); // handled in dispatcher
        UNUSED(PAR(lines)); // handled in dispatcher

        REBFLGS flags = 0;

        // Handle the READ %file shortcut case, where the FILE! has been
        // converted into a PORT! but has not been opened yet.

        bool opened;
        if (req->flags & RRF_OPEN)
            opened = false; // was already open
        else {
            REBLEN nargs = AM_OPEN_READ;
            if (REF(seek))
                nargs |= AM_OPEN_SEEK;
            Setup_File(file, nargs, path);
            Open_File_Port(port, file, path);
            opened = true; // had to be opened (shortcut case)
        }

        if (REF(seek))
            Set_Seek(file, ARG(seek));

        REBLEN len = Set_Length(file, REF(part) ? VAL_INT64(ARG(part)) : -1);
        Read_File_Port(D_OUT, port, file, path, flags, len);

        if (opened) {
            REBVAL *result = OS_DO_DEVICE(file, RDC_CLOSE);
            assert(result != NULL); // should be synchronous

            Cleanup_File(file);

            if (rebDid("error?", result, rebEND))
                rebJumps("FAIL", result, rebEND);

            rebRelease(result); // ignore result
        }

        return D_OUT; }

      case SYM_APPEND:
        //
        // !!! This is hacky, but less hacky than falling through to SYM_WRITE
        // assuming the frame is the same for APPEND and WRITE (which is what
        // R3-Alpha did).  Review.
        //
        return Retrigger_Append_As_Write(frame_);

      case SYM_WRITE: {
        INCLUDE_PARAMS_OF_WRITE;

        UNUSED(PAR(destination));

        if (REF(allow))
            fail (Error_Bad_Refines_Raw());

        REBVAL *data = ARG(data); // binary, string, or block

        // Handle the WRITE %file shortcut case, where the FILE! is converted
        // to a PORT! but it hasn't been opened yet.

        bool opened;
        if (req->flags & RRF_OPEN) {
            if (not (req->modes & RFM_WRITE))
                fail (Error_Read_Only_Raw(path));

            opened = false; // already open
        }
        else {
            REBLEN nargs = AM_OPEN_WRITE;
            if (REF(seek) || REF(append))
                nargs |= AM_OPEN_SEEK;
            else
                nargs |= AM_OPEN_NEW;
            Setup_File(file, nargs, path);
            Open_File_Port(port, file, path);
            opened = true;
        }

        if (REF(append)) {
            ReqFile(file)->index = -1; // append
            req->modes |= RFM_RESEEK;
        }
        if (REF(seek))
            Set_Seek(file, ARG(seek));

        // Determine length. Clip /PART to size of string if needed.
        REBLEN len = VAL_LEN_AT(data);
        if (REF(part)) {
            REBLEN n = Int32s(ARG(part), 0);
            if (n <= len)
                len = n;
        }

        Write_File_Port(file, data, len, did REF(lines));

        if (opened) {
            REBVAL *result = OS_DO_DEVICE(file, RDC_CLOSE);
            assert(result != NULL); // should be synchronous

            Cleanup_File(file);

            if (rebDid("error?", result, rebEND))
                rebJumps("FAIL", result, rebEND);

            rebRelease(result);
        }

        RETURN (port); }

      case SYM_OPEN: {
        INCLUDE_PARAMS_OF_OPEN;

        UNUSED(PAR(spec));

        if (REF(allow))
            fail (Error_Bad_Refines_Raw());

        REBFLGS flags = (
            (REF(new) ? AM_OPEN_NEW : 0)
            | (REF(read) or not REF(write) ? AM_OPEN_READ : 0)
            | (REF(write) or not REF(read) ? AM_OPEN_WRITE : 0)
            | (REF(seek) ? AM_OPEN_SEEK : 0)
            | (REF(allow) ? AM_OPEN_ALLOW : 0)
        );
        Setup_File(file, flags, path);

        // !!! need to change file modes to R/O if necessary

        Open_File_Port(port, file, path);

        RETURN (port); }

      case SYM_COPY: {
        INCLUDE_PARAMS_OF_COPY;

        UNUSED(PAR(value));

        if (REF(deep) or REF(types))
            fail (Error_Bad_Refines_Raw());

        if (not (req->flags & RRF_OPEN))
            fail (Error_Not_Open_Raw(path)); // !!! wrong msg

        REBLEN len = Set_Length(file, REF(part) ? VAL_INT64(ARG(part)) : -1);
        REBFLGS flags = 0;
        Read_File_Port(D_OUT, port, file, path, flags, len);
        return D_OUT; }

      case SYM_CLOSE: {
        INCLUDE_PARAMS_OF_CLOSE;
        UNUSED(PAR(port));

        if (req->flags & RRF_OPEN) {
            REBVAL *result = OS_DO_DEVICE(file, RDC_CLOSE);
            assert(result != NULL); // should be synchronous

            Cleanup_File(file);

            if (rebDid("error?", result, rebEND))
                rebJumps("FAIL", result, rebEND);

            rebRelease(result); // ignore error
        }
        RETURN (port); }

      case SYM_DELETE: {
        INCLUDE_PARAMS_OF_DELETE;
        UNUSED(PAR(port));

        if (req->flags & RRF_OPEN)
            fail (Error_No_Delete_Raw(path));
        Setup_File(file, 0, path);

        REBVAL *result = OS_DO_DEVICE(file, RDC_DELETE);
        assert(result != NULL); // should be synchronous

        if (rebDid("error?", result, rebEND))
            rebJumps("FAIL", result, rebEND);

        rebRelease(result); // ignore result
        RETURN (port); }

      case SYM_RENAME: {
        INCLUDE_PARAMS_OF_RENAME;

        if (req->flags & RRF_OPEN)
            fail (Error_No_Rename_Raw(path));

        Setup_File(file, 0, path);

        req->common.data = cast(REBYTE*, ARG(to)); // !!! hack!

        REBVAL *result = OS_DO_DEVICE(file, RDC_RENAME);
        assert(result != NULL); // should be synchronous
        if (rebDid("error?", result, rebEND))
            rebJumps("FAIL", result, rebEND);
        rebRelease(result); // ignore result

        RETURN (ARG(from)); }

      case SYM_CREATE: {
        if (not (req->flags & RRF_OPEN)) {
            Setup_File(file, AM_OPEN_WRITE | AM_OPEN_NEW, path);

            REBVAL *cr_result = OS_DO_DEVICE(file, RDC_CREATE);
            assert(cr_result != NULL);
            if (rebDid("error?", cr_result, rebEND))
                rebJumps("FAIL", cr_result, rebEND);
            rebRelease(cr_result);

            REBVAL *cl_result = OS_DO_DEVICE(file, RDC_CLOSE);
            assert(cl_result != NULL);
            if (rebDid("error?", cl_result, rebEND))
                rebJumps("FAIL", cl_result, rebEND);
            rebRelease(cl_result);
        }

        // !!! should it leave file open???

        RETURN (port); }

      case SYM_QUERY: {
        INCLUDE_PARAMS_OF_QUERY;

        UNUSED(PAR(target));

        if (REF(mode))
            fail (Error_Bad_Refines_Raw());

        if (not (req->flags & RRF_OPEN)) {
            Setup_File(file, 0, path);
            REBVAL *result = OS_DO_DEVICE(file, RDC_QUERY);
            assert(result != NULL);
            if (rebDid("error?", result, rebEND)) {
                rebRelease(result); // !!! R3-Alpha returned blank on error
                return nullptr;
            }
            rebRelease(result); // ignore result
        }

        // !!! free file path?

        return Query_File_Or_Dir(port, file); }

      case SYM_SKIP: {
        INCLUDE_PARAMS_OF_SKIP;

        UNUSED(PAR(series));
        UNUSED(REF(unbounded));  // !!! Should /UNBOUNDED behave differently?

        ReqFile(file)->index += Get_Num_From_Arg(ARG(offset));
        req->modes |= RFM_RESEEK;
        RETURN (port); }

      case SYM_CLEAR: {
        // !! check for write enabled?
        req->modes |= RFM_RESEEK;
        req->modes |= RFM_TRUNCATE;
        req->length = 0;

        OS_DO_DEVICE_SYNC(file, RDC_WRITE);
        RETURN (port); }

      default:
        break;
    }

    return R_UNHANDLED;
}
