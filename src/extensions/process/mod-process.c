//
//  File: %mod-call.c
//  Summary: "Native Functions for spawning and controlling processes"
//  Section: Extension
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 Atronix Engineering
// Copyright 2012-2017 Rebol Open Source Contributors
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
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//=////////////////////////////////////////////////////////////////////////=//
//

#ifdef TO_WINDOWS
    #include <windows.h>
    #include <process.h>
    #include <shlobj.h>

    #ifdef IS_ERROR
        #undef IS_ERROR //winerror.h defines, Rebol has a different meaning
    #endif
#else
    #if !defined( __cplusplus) && defined(TO_LINUX)
        //
        // See feature_test_macros(7), this definition is redundant under C++
        //
        #define _GNU_SOURCE // Needed for pipe2 when #including <unistd.h>
    #endif
    #include <unistd.h>

    #include <errno.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <signal.h>
    #include <sys/stat.h>
    #include <sys/wait.h>
#endif

#include "sys-core.h"
#include "sys-ext.h"

#include "tmp-mod-process-first.h"


// !!! The original implementation of CALL from Atronix had to communicate
// between the CALL native (defined in the core) and the host routine
// OS_Create_Process, which was not designed to operate on Rebol types.
// Hence if the user was passing in a BINARY! to which the data for the
// standard out or standard error was to be saved, it was produced in full
// in a buffer and returned, then appended.  This wastes space when compared
// to just appending to the string or binary itself.  With CALL rethought
// as an extension with access to the internal API, this could be changed...
// though for the moment, a malloc()'d buffer is expanded independently by
// BUF_SIZE_CHUNK and returned to CALL.
//
#define BUF_SIZE_CHUNK 4096


#ifdef TO_WINDOWS
//
//  OS_Create_Process: C
//
// Return -1 on error.
//
int OS_Create_Process(
    REBFRM *frame_, // stopgap: allows access to CALL's ARG() and REF()
    const wchar_t *call,
    int argc,
    const wchar_t * argv[],
    REBOOL flag_wait,
    u64 *pid,
    int *exit_code,
    char *input,
    u32 input_len,
    char **output,
    u32 *output_len,
    char **err,
    u32 *err_len
) {
    INCLUDE_PARAMS_OF_CALL;

    UNUSED(ARG(command)); // turned into `call` and `argv/argc` by CALL
    UNUSED(REF(wait)); // covered by flag_wait

    UNUSED(REF(console)); // actually not paid attention to

    if (call == NULL)
        fail ("'argv[]'-style launching not implemented on Windows CALL");

#ifdef GET_IS_NT_FLAG // !!! Why was this here?
    REBOOL is_NT;
    OSVERSIONINFO info;
    GetVersionEx(&info);
    is_NT = info.dwPlatformId >= VER_PLATFORM_WIN32_NT;
#endif

    UNUSED(argc);
    UNUSED(argv);

    REBINT result = -1;
    REBINT ret = 0;
    HANDLE hOutputRead = 0, hOutputWrite = 0;
    HANDLE hInputWrite = 0, hInputRead = 0;
    HANDLE hErrorWrite = 0, hErrorRead = 0;
    wchar_t *cmd = NULL;
    char *oem_input = NULL;

    UNUSED(REF(info));

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    STARTUPINFO si;
    si.cb = sizeof(si);
    si.lpReserved = NULL;
    si.lpDesktop = NULL;
    si.lpTitle = NULL;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.wShowWindow = SW_SHOWNORMAL;
    si.cbReserved2 = 0;
    si.lpReserved2 = NULL;

    UNUSED(REF(input)); // implicitly covered by void ARG(in)
    switch (VAL_TYPE(ARG(in))) {
    case REB_STRING:
    case REB_BINARY:
        if (!CreatePipe(&hInputRead, &hInputWrite, NULL, 0)) {
            goto input_error;
        }

        // make child side handle inheritable
        if (!SetHandleInformation(
            hInputRead, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT
        )){
            goto input_error;
        }
        si.hStdInput = hInputRead;
        break;

    case REB_FILE: {
        REBSER *path = Value_To_OS_Path(ARG(in), FALSE);

        hInputRead = CreateFile(
            SER_HEAD(wchar_t, path),
            GENERIC_READ, // desired mode
            0, // shared mode
            &sa, // security attributes
            OPEN_EXISTING, // creation disposition
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, // flags
            NULL // template
        );
        si.hStdInput = hInputRead;

        Free_Series(path);
        break; }

    case REB_BLANK:
        si.hStdInput = 0;
        break;

    case REB_MAX_VOID:
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        break;

    default:
        panic (ARG(in));
    }

    UNUSED(REF(output)); // implicitly covered by void ARG(out)
    switch (VAL_TYPE(ARG(out))) {
    case REB_STRING:
    case REB_BINARY:
        if (!CreatePipe(&hOutputRead, &hOutputWrite, NULL, 0)) {
            goto output_error;
        }

        // make child side handle inheritable
        //
        if (!SetHandleInformation(
            hOutputWrite, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT
        )){
            goto output_error;
        }
        si.hStdOutput = hOutputWrite;
        break;

    case REB_FILE: {
        REBSER *path = Value_To_OS_Path(ARG(out), FALSE);

        si.hStdOutput = CreateFile(
            SER_HEAD(wchar_t, path),
            GENERIC_WRITE, // desired mode
            0, // shared mode
            &sa, // security attributes
            CREATE_NEW, // creation disposition
            FILE_ATTRIBUTE_NORMAL, // flag and attributes
            NULL // template
        );

        if (
            si.hStdOutput == INVALID_HANDLE_VALUE
            && GetLastError() == ERROR_FILE_EXISTS
        ){
            si.hStdOutput = CreateFile(
                SER_HEAD(wchar_t, path),
                GENERIC_WRITE, // desired mode
                0, // shared mode
                &sa, // security attributes
                OPEN_EXISTING, // creation disposition
                FILE_ATTRIBUTE_NORMAL, // flag and attributes
                NULL // template
            );
        }

        Free_Series(path);
        break; }

    case REB_BLANK:
        si.hStdOutput = 0;
        break;

    case REB_MAX_VOID:
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        break;

    default:
        panic (ARG(out));
    }

    UNUSED(REF(error)); // implicitly covered by void ARG(err)
    switch (VAL_TYPE(ARG(err))) {
    case REB_STRING:
    case REB_BINARY:
        if (!CreatePipe(&hErrorRead, &hErrorWrite, NULL, 0)) {
            goto error_error;
        }

        // make child side handle inheritable
        //
        if (!SetHandleInformation(
            hErrorWrite, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT
        )){
            goto error_error;
        }
        si.hStdError = hErrorWrite;
        break;

    case REB_FILE: {
        REBSER *path = Value_To_OS_Path(ARG(out), FALSE);

        si.hStdError = CreateFile(
            SER_HEAD(wchar_t, path),
            GENERIC_WRITE, // desired mode
            0, // shared mode
            &sa, // security attributes
            CREATE_NEW, // creation disposition
            FILE_ATTRIBUTE_NORMAL, // flag and attributes
            NULL // template
        );

        if (
            si.hStdError == INVALID_HANDLE_VALUE
            && GetLastError() == ERROR_FILE_EXISTS
        ){
            si.hStdError = CreateFile(
                SER_HEAD(wchar_t, path),
                GENERIC_WRITE, // desired mode
                0, // shared mode
                &sa, // security attributes
                OPEN_EXISTING, // creation disposition
                FILE_ATTRIBUTE_NORMAL, // flag and attributes
                NULL // template
            );
        }

        Free_Series(path);
        break; }

    case REB_BLANK:
        si.hStdError = 0;
        break;

    case REB_MAX_VOID:
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        break;

    default:
        panic (ARG(err));
    }

    if (REF(shell)) {
        const wchar_t *sh = L"cmd.exe /C ";
        size_t len = wcslen(sh) + wcslen(call) + 1;

        cmd = cast(wchar_t*, malloc(len * sizeof(wchar_t)));
        cmd[0] = L'\0';
        wcscat(cmd, sh);
        wcscat(cmd, call);
    }
    else {
        // CreateProcess might write to this memory
        // Duplicate it to be safe
        cmd = _wcsdup(call);
    }

    PROCESS_INFORMATION pi;
    result = CreateProcess(
        NULL, // executable name
        cmd, // command to execute
        NULL, // process security attributes
        NULL, // thread security attributes
        TRUE, // inherit handles, must be TRUE for I/O redirection
        NORMAL_PRIORITY_CLASS | CREATE_DEFAULT_ERROR_MODE, // creation flags
        NULL, // environment
        NULL, // current directory
        &si, // startup information
        &pi // process information
    );

    free(cmd);

    *pid = pi.dwProcessId;

    if (hInputRead != NULL)
        CloseHandle(hInputRead);

    if (hOutputWrite != NULL)
        CloseHandle(hOutputWrite);

    if (hErrorWrite != NULL)
        CloseHandle(hErrorWrite);

    // Wait for termination:
    if (result != 0 && flag_wait) {
        HANDLE handles[3];
        int count = 0;
        DWORD output_size = 0;
        DWORD err_size = 0;

        if (hInputWrite != NULL && input_len > 0) {
            if (IS_STRING(ARG(in))) {
                DWORD dest_len = 0;
                /* convert input encoding from UNICODE to OEM */
                // !!! Is cast to wchar_t here legal?
                dest_len = WideCharToMultiByte(
                    CP_OEMCP,
                    0,
                    cast(wchar_t*, input),
                    input_len,
                    oem_input,
                    dest_len,
                    NULL,
                    NULL
                );
                if (dest_len > 0) {
                    oem_input = cast(char*, malloc(dest_len));
                    if (oem_input != NULL) {
                        WideCharToMultiByte(
                            CP_OEMCP,
                            0,
                            cast(wchar_t*, input),
                            input_len,
                            oem_input,
                            dest_len,
                            NULL,
                            NULL
                        );
                        input_len = dest_len;
                        input = oem_input;
                        handles[count ++] = hInputWrite;
                    }
                }
            } else {
                assert(IS_BINARY(ARG(in)));
                handles[count ++] = hInputWrite;
            }
        }
        if (hOutputRead != NULL) {
            output_size = BUF_SIZE_CHUNK;
            *output_len = 0;

            *output = cast(char*, malloc(output_size));
            handles[count ++] = hOutputRead;
        }
        if (hErrorRead != NULL) {
            err_size = BUF_SIZE_CHUNK;
            *err_len = 0;

            *err = cast(char*, malloc(err_size));
            handles[count++] = hErrorRead;
        }

        while (count > 0) {
            DWORD wait_result = WaitForMultipleObjects(
                count, handles, FALSE, INFINITE
            );

            // If we test wait_result >= WAIT_OBJECT_0 it will tell us "always
            // true" with -Wtype-limits, since WAIT_OBJECT_0 is 0.  Take that
            // comparison out but add assert in case you're on some abstracted
            // Windows and it isn't 0 for that implementation.
            //
            assert(WAIT_OBJECT_0 == 0);
            if (wait_result < WAIT_OBJECT_0 + count) {
                int i = wait_result - WAIT_OBJECT_0;
                DWORD input_pos = 0;
                DWORD n = 0;

                if (handles[i] == hInputWrite) {
                    if (!WriteFile(
                        hInputWrite,
                        cast(char*, input) + input_pos,
                        input_len - input_pos,
                        &n,
                        NULL
                    )){
                        if (i < count - 1) {
                            memmove(
                                &handles[i],
                                &handles[i + 1],
                                (count - i - 1) * sizeof(HANDLE)
                            );
                        }
                        count--;
                    }
                    else {
                        input_pos += n;
                        if (input_pos >= input_len) {
                            /* done with input */
                            CloseHandle(hInputWrite);
                            hInputWrite = NULL;
                            free(oem_input);
                            oem_input = NULL;
                            if (i < count - 1) {
                                memmove(
                                    &handles[i],
                                    &handles[i + 1],
                                    (count - i - 1) * sizeof(HANDLE)
                                );
                            }
                            count--;
                        }
                    }
                }
                else if (handles[i] == hOutputRead) {
                    if (!ReadFile(
                        hOutputRead,
                        *cast(char**, output) + *output_len,
                        output_size - *output_len,
                        &n,
                        NULL
                    )){
                        if (i < count - 1) {
                            memmove(
                                &handles[i],
                                &handles[i + 1],
                                (count - i - 1) * sizeof(HANDLE)
                            );
                        }
                        count--;
                    }
                    else {
                        *output_len += n;
                        if (*output_len >= output_size) {
                            output_size += BUF_SIZE_CHUNK;
                            *output = cast(char*, realloc(*output, output_size));
                            if (*output == NULL) goto kill;
                        }
                    }
                }
                else if (handles[i] == hErrorRead) {
                    if (!ReadFile(
                        hErrorRead,
                        *cast(char**, err) + *err_len,
                        err_size - *err_len,
                        &n,
                        NULL
                    )){
                        if (i < count - 1) {
                            memmove(
                                &handles[i],
                                &handles[i + 1],
                                (count - i - 1) * sizeof(HANDLE)
                            );
                        }
                        count--;
                    }
                    else {
                        *err_len += n;
                        if (*err_len >= err_size) {
                            err_size += BUF_SIZE_CHUNK;
                            *err = cast(char*, realloc(*err, err_size));
                            if (*err == NULL) goto kill;
                        }
                    }
                }
                else {
                    //printf("Error READ");
                    if (!ret) ret = GetLastError();
                    goto kill;
                }
            }
            else if (wait_result == WAIT_FAILED) { /* */
                //printf("Wait Failed\n");
                if (!ret) ret = GetLastError();
                goto kill;
            }
            else {
                //printf("Wait returns unexpected result: %d\n", wait_result);
                if (!ret) ret = GetLastError();
                goto kill;
            }
        }

        WaitForSingleObject(pi.hProcess, INFINITE); // check result??

        DWORD temp;
        GetExitCodeProcess(pi.hProcess, &temp);
        *exit_code = temp;

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        if (IS_STRING(ARG(out)) && *output != NULL && *output_len > 0) {
            /* convert to wide char string */
            int dest_len = 0;
            wchar_t *dest = NULL;
            dest_len = MultiByteToWideChar(
                CP_OEMCP, 0, *output, *output_len, dest, 0
            );
            if (dest_len <= 0) {
                free(*output);
                *output = NULL;
                *output_len = 0;
            }
            dest = cast(wchar_t*, malloc(*output_len * sizeof(wchar_t)));
            if (dest == NULL)
                goto cleanup;
            MultiByteToWideChar(
                CP_OEMCP, 0, *output, *output_len, dest, dest_len
            );
            free(*output);
            *output = cast(char*, dest);
            *output_len = dest_len;
        }

        if (IS_STRING(ARG(err)) && *err != NULL && *err_len > 0) {
            /* convert to wide char string */
            int dest_len = 0;
            wchar_t *dest = NULL;
            dest_len = MultiByteToWideChar(
                CP_OEMCP, 0, *err, *err_len, dest, 0
            );
            if (dest_len <= 0) {
                free(*err);
                *err = NULL;
                *err_len = 0;
            }
            dest = cast(wchar_t*, malloc(*err_len * sizeof(wchar_t)));
            if (dest == NULL) goto cleanup;
            MultiByteToWideChar(CP_OEMCP, 0, *err, *err_len, dest, dest_len);
            free(*err);
            *err = cast(char*, dest);
            *err_len = dest_len;
        }
    } else if (result) {
        //
        // No wait, close handles to avoid leaks
        //
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    else {
        // CreateProcess failed
        ret = GetLastError();
    }

    goto cleanup;

kill:
    if (TerminateProcess(pi.hProcess, 0)) {
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD temp;
        GetExitCodeProcess(pi.hProcess, &temp);
        *exit_code = temp;
    }
    else if (ret == 0) {
        ret = GetLastError();
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

cleanup:
    if (oem_input != NULL) {
        free(oem_input);
    }

    if (output != NULL && *output != NULL && *output_len == 0) {
        free(*output);
    }

    if (err != NULL && *err != NULL && *err_len == 0) {
        free(*err);
    }

    if (hInputWrite != NULL)
        CloseHandle(hInputWrite);

    if (hOutputRead != NULL)
        CloseHandle(hOutputRead);

    if (hErrorRead != NULL)
        CloseHandle(hErrorRead);

    if (IS_FILE(ARG(err))) {
        CloseHandle(si.hStdError);
    }

error_error:
    if (IS_FILE(ARG(out))) {
        CloseHandle(si.hStdOutput);
    }

output_error:
    if (IS_FILE(ARG(in))) {
        CloseHandle(si.hStdInput);
    }

input_error:
    return ret;  // meaning depends on flags
}

#else // !defined(TO_WINDOWS), so POSIX, LINUX, OS X, etc.

static REBOOL Open_Nonblocking_Pipe_Fails(int pipefd[2]) {
#ifdef USE_PIPE2_NOT_PIPE
    //
    // NOTE: pipe() is POSIX, but pipe2() is Linux-specific.  With pipe() it
    // takes an additional call to fcntl() to request non-blocking behavior,
    // so it's a small amount more work.  However, there are other flags which
    // if aren't passed atomically at the moment of opening allow for a race
    // condition in threading if split, e.g. FD_CLOEXEC.
    //
    // (If you don't have FD_CLOEXEC set on the file descriptor, then all
    // instances of CALL will act as a /WAIT.)
    //
    // At time of writing, this is mostly academic...but the code needed to be
    // patched to work with pipe() since some older libcs do not have pipe2().
    // So the ability to target both are kept around, saving the pipe2() call
    // for later Linuxes known to have it (and O_CLOEXEC).
    //
    if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK))
        return TRUE;
#else
    if (pipe(pipefd) < 0)
        return TRUE;

    int direction; // READ=0, WRITE=1
    for (direction = 0; direction < 2; ++direction) {
        int oldflags;
        oldflags = fcntl(pipefd[direction], F_GETFL);
        if (oldflags < 0)
            return TRUE;
        if (fcntl(pipefd[direction], F_SETFL, oldflags | O_NONBLOCK) < 0)
            return TRUE;
        oldflags = fcntl(pipefd[direction], F_GETFD);
        if (oldflags < 0)
            return TRUE;
        if (fcntl(pipefd[direction], F_SETFD, oldflags | FD_CLOEXEC) < 0)
            return TRUE;
    }
#endif

    return FALSE;
}


//
//  OS_Create_Process: C
//
// flags:
//     1: wait, is implied when I/O redirection is enabled
//     2: console
//     4: shell
//     8: info
//     16: show
//
// Return -1 on error, otherwise the process return code.
//
// POSIX previous simple version was just 'return system(call);'
// This uses 'execvp' which is "POSIX.1 conforming, UNIX compatible"
//
int OS_Create_Process(
    REBFRM *frame_, // stopgap: allows access to CALL's ARG() and REF()
    const char *call,
    int argc,
    const char* argv[],
    REBOOL flag_wait, // distinct from REF(wait)
    u64 *pid,
    int *exit_code,
    char *input,
    u32 input_len,
    char **output,
    u32 *output_len,
    char **err,
    u32 *err_len
) {
    INCLUDE_PARAMS_OF_CALL;

    UNUSED(ARG(command)); // translated into call and argc/argv
    UNUSED(REF(wait)); // flag_wait controls this
    UNUSED(REF(input));
    UNUSED(REF(output));
    UNUSED(REF(error));

    UNUSED(REF(console)); // actually not paid attention to

    UNUSED(call);

    int status = 0;
    int ret = 0;
    char *info = NULL;
    off_t info_size = 0;
    u32 info_len = 0;
    pid_t fpid = 0;

    const unsigned int R = 0;
    const unsigned int W = 1;

    // We want to be able to compile with all warnings as errors, and
    // we'd like to use -Wcast-qual if possible.  This is currently
    // the only barrier in the codebase...so we tunnel under the cast.
    //
    char * const *argv_hack;

    // suppress unused warnings but keep flags for future use
    UNUSED(REF(info));
    UNUSED(REF(console));

    int stdin_pipe[] = {-1, -1};
    int stdout_pipe[] = {-1, -1};
    int stderr_pipe[] = {-1, -1};
    int info_pipe[] = {-1, -1};

    if (IS_STRING(ARG(in)) || IS_BINARY(ARG(in))) {
        if (Open_Nonblocking_Pipe_Fails(stdin_pipe))
            goto stdin_pipe_err;
    }

    if (IS_STRING(ARG(out)) || IS_BINARY(ARG(out))) {
        if (Open_Nonblocking_Pipe_Fails(stdout_pipe))
            goto stdout_pipe_err;
    }

    if (IS_STRING(ARG(err)) || IS_BINARY(ARG(err))) {
        if (Open_Nonblocking_Pipe_Fails(stderr_pipe))
            goto stdout_pipe_err;
    }

    if (Open_Nonblocking_Pipe_Fails(info_pipe))
        goto info_pipe_err;

    fpid = fork();
    if (fpid == 0) {
        //
        // This is the child branch of the fork.  In GDB if you want to debug
        // the child you need to use `set follow-fork-mode child`:
        //
        // http://stackoverflow.com/questions/15126925/

        if (IS_STRING(ARG(in)) || IS_BINARY(ARG(in))) {
            close(stdin_pipe[W]);
            if (dup2(stdin_pipe[R], STDIN_FILENO) < 0)
                goto child_error;
            close(stdin_pipe[R]);
        }
        else if (IS_FILE(ARG(in))) {
            REBSER *path = Value_To_OS_Path(ARG(in), FALSE);
            int fd = open(SER_HEAD(char, path), O_RDONLY);
            Free_Series(path);

            if (fd < 0)
                goto child_error;
            if (dup2(fd, STDIN_FILENO) < 0)
                goto child_error;
            close(fd);
        }
        else if (IS_BLANK(ARG(in))) {
            int fd = open("/dev/null", O_RDONLY);
            if (fd < 0)
                goto child_error;
            if (dup2(fd, STDIN_FILENO) < 0)
                goto child_error;
            close(fd);
        }
        else {
            assert(IS_VOID(ARG(in)));
            // inherit stdin from the parent
        }

        if (IS_STRING(ARG(out)) || IS_BINARY(ARG(out))) {
            close(stdout_pipe[R]);
            if (dup2(stdout_pipe[W], STDOUT_FILENO) < 0)
                goto child_error;
            close(stdout_pipe[W]);
        }
        else if (IS_FILE(ARG(out))) {
            REBSER *path = Value_To_OS_Path(ARG(out), FALSE);
            int fd = open(SER_HEAD(char, path), O_CREAT | O_WRONLY, 0666);
            Free_Series(path);

            if (fd < 0)
                goto child_error;
            if (dup2(fd, STDOUT_FILENO) < 0)
                goto child_error;
            close(fd);
        }
        else if (IS_BLANK(ARG(out))) {
            int fd = open("/dev/null", O_WRONLY);
            if (fd < 0)
                goto child_error;
            if (dup2(fd, STDOUT_FILENO) < 0)
                goto child_error;
            close(fd);
        }
        else {
            assert(IS_VOID(ARG(out)));
            // inherit stdout from the parent
        }

        if (IS_STRING(ARG(err)) || IS_BINARY(ARG(err))) {
            close(stderr_pipe[R]);
            if (dup2(stderr_pipe[W], STDERR_FILENO) < 0)
                goto child_error;
            close(stderr_pipe[W]);
        }
        else if (IS_FILE(ARG(err))) {
            REBSER *path = Value_To_OS_Path(ARG(err), FALSE);
            int fd = open(SER_HEAD(char, path), O_CREAT | O_WRONLY, 0666);
            Free_Series(path);

            if (fd < 0)
                goto child_error;
            if (dup2(fd, STDERR_FILENO) < 0)
                goto child_error;
            close(fd);
        }
        else if (IS_BLANK(ARG(err))) {
            int fd = open("/dev/null", O_WRONLY);
            if (fd < 0)
                goto child_error;
            if (dup2(fd, STDERR_FILENO) < 0)
                goto child_error;
            close(fd);
        }
        else {
            assert(IS_VOID(ARG(err)));
            // inherit stderr from the parent
        }

        close(info_pipe[R]);

        /* printf("flag_shell in child: %hhu\n", flag_shell); */

        if (REF(shell)) {
            const char *sh = getenv("SHELL");

            if (sh == NULL) { // shell does not exist
                int err = 2;
                if (write(info_pipe[W], &err, sizeof(err)) == -1) {
                    //
                    // Nothing we can do, but need to stop compiler warning
                    // (cast to void is insufficient for warn_unused_result)
                }
                exit(EXIT_FAILURE);
            }

            const char ** argv_new = cast(const char**, malloc(argc + 3));
            argv_new[0] = sh;
            argv_new[1] = "-c";
            memcpy(&argv_new[2], argv, argc * sizeof(argv[0]));
            argv_new[argc + 2] = NULL;

            memcpy(&argv_hack, &argv_new, sizeof(argv_hack));
            execvp(sh, argv_hack);
        }
        else {
            memcpy(&argv_hack, &argv, sizeof(argv_hack));
            execvp(argv[0], argv_hack);
        }

child_error: ;
        //
        // The original implementation of this code would write errno to the
        // info pipe.  However, errno may be volatile (and it is on Android).
        // write() does not accept volatile pointers, so copy it to a
        // temporary value first.
        //
        int nonvolatile_errno = errno;

        if (write(info_pipe[W], &nonvolatile_errno, sizeof(int)) == -1) {
            //
            // Nothing we can do, but need to stop compiler warning
            // (cast to void is insufficient for warn_unused_result)
        }
        exit(EXIT_FAILURE); /* get here only when exec fails */
    }
    else if (fpid > 0) {
        //
        // This is the parent branch, so it may (or may not) wait on the
        // child fork branch, based on /WAIT.  Even if you are not using
        // /WAIT, it will use the info pipe to make sure the process did
        // actually start.
        //
        nfds_t nfds = 0;
        struct pollfd pfds[4];
        pid_t xpid;
        unsigned int i;
        ssize_t nbytes;
        off_t input_size = 0;
        off_t output_size = 0;
        off_t err_size = 0;
        int valid_nfds;

        // Only put the input pipe in the consideration if we can write to
        // it and we have data to send to it.

        if ((stdin_pipe[W] > 0) && (input_size = strlen(input)) > 0) {
            /* printf("stdin_pipe[W]: %d\n", stdin_pipe[W]); */

            //
            // the passed in input_len is in characters, not in bytes
            //
            input_len = 0;

            pfds[nfds].fd = stdin_pipe[W];
            pfds[nfds].events = POLLOUT;
            nfds++;

            close(stdin_pipe[R]);
            stdin_pipe[R] = -1;
        }
        if (stdout_pipe[R] > 0) {
            /* printf("stdout_pipe[R]: %d\n", stdout_pipe[R]); */

            output_size = BUF_SIZE_CHUNK;

            *output = cast(char*, malloc(output_size));
            *output_len = 0;

            pfds[nfds].fd = stdout_pipe[R];
            pfds[nfds].events = POLLIN;
            nfds++;

            close(stdout_pipe[W]);
            stdout_pipe[W] = -1;
        }
        if (stderr_pipe[R] > 0) {
            /* printf("stderr_pipe[R]: %d\n", stderr_pipe[R]); */

            err_size = BUF_SIZE_CHUNK;

            *err = cast(char*, malloc(err_size));
            *err_len = 0;

            pfds[nfds].fd = stderr_pipe[R];
            pfds[nfds].events = POLLIN;
            nfds++;

            close(stderr_pipe[W]);
            stderr_pipe[W] = -1;
        }

        if (info_pipe[R] > 0) {
            pfds[nfds].fd = info_pipe[R];
            pfds[nfds].events = POLLIN;
            nfds++;

            info_size = 4;

            info = cast(char*, malloc(info_size));

            close(info_pipe[W]);
            info_pipe[W] = -1;
        }

        valid_nfds = nfds;
        while (valid_nfds > 0) {
            xpid = waitpid(fpid, &status, WNOHANG);
            if (xpid == -1) {
                ret = errno;
                goto error;
            }

            if (xpid == fpid) {
                //
                // try one more time to read any remainding output/err
                //
                if (stdout_pipe[R] > 0) {
                    nbytes = read(
                        stdout_pipe[R],
                        *output + *output_len,
                        output_size - *output_len
                    );

                    if (nbytes > 0) {
                        *output_len += nbytes;
                    }
                }

                if (stderr_pipe[R] > 0) {
                    nbytes = read(
                        stderr_pipe[R],
                        *err + *err_len,
                        err_size - *err_len
                    );
                    if (nbytes > 0) {
                        *err_len += nbytes;
                    }
                }

                if (info_pipe[R] > 0) {
                    nbytes = read(
                        info_pipe[R],
                        info + info_len,
                        info_size - info_len
                    );
                    if (nbytes > 0) {
                        info_len += nbytes;
                    }
                }

                break;
            }

            /*
            for (i = 0; i < nfds; ++i) {
                printf(" %d", pfds[i].fd);
            }
            printf(" / %d\n", nfds);
            */
            if (poll(pfds, nfds, -1) < 0) {
                ret = errno;
                goto kill;
            }

            for (i = 0; i < nfds && valid_nfds > 0; ++i) {
                /* printf("check: %d [%d/%d]\n", pfds[i].fd, i, nfds); */

                if (pfds[i].revents & POLLERR) {
                    /* printf("POLLERR: %d [%d/%d]\n", pfds[i].fd, i, nfds); */

                    close(pfds[i].fd);
                    pfds[i].fd = -1;
                    valid_nfds --;
                }
                else if (pfds[i].revents & POLLOUT) {
                    /* printf("POLLOUT: %d [%d/%d]\n", pfds[i].fd, i, nfds); */

                    nbytes = write(pfds[i].fd, input, input_size - input_len);
                    if (nbytes <= 0) {
                        ret = errno;
                        goto kill;
                    }
                    /* printf("POLLOUT: %d bytes\n", nbytes); */
                    input_len += nbytes;
                    if (cast(off_t, input_len) >= input_size) {
                        close(pfds[i].fd);
                        pfds[i].fd = -1;
                        valid_nfds --;
                    }
                }
                else if (pfds[i].revents & POLLIN) {
                    /* printf("POLLIN: %d [%d/%d]\n", pfds[i].fd, i, nfds); */
                    char **buffer = NULL;
                    u32 *offset;
                    ssize_t to_read = 0;
                    size_t size;
                    if (pfds[i].fd == stdout_pipe[R]) {
                        buffer = (char**)output;
                        offset = output_len;
                        size = output_size;
                    } else if (pfds[i].fd == stderr_pipe[R]) {
                        buffer = (char**)err;
                        offset = err_len;
                        size = err_size;
                    } else { /* info pipe */
                        buffer = &info;
                        offset = &info_len;
                        size = info_size;
                    }
                    do {
                        to_read = size - *offset;
                        /* printf("to read %d bytes\n", to_read); */
                        nbytes = read(pfds[i].fd, *buffer + *offset, to_read);
                        if (nbytes < 0) {
                            break;
                        }
                        if (nbytes == 0) { // closed
                            /* printf("the other end closed\n"); */
                            close(pfds[i].fd);
                            pfds[i].fd = -1;
                            valid_nfds --;
                            break;
                        }
                        /* printf("POLLIN: %d bytes\n", nbytes); */
                        *offset += nbytes;
                        if (*offset >= size) {
                            char *larger =
                                cast(char*, size + BUF_SIZE_CHUNK);
                            if (!larger) goto kill;
                            memcpy(larger, *buffer, size * sizeof(larger[0]));
                            free(*buffer);
                            *buffer = larger;
                            size += BUF_SIZE_CHUNK;
                        }
                    } while (nbytes == to_read);
                }
                else if (pfds[i].revents & POLLHUP) {
                    /* printf("POLLHUP: %d [%d/%d]\n", pfds[i].fd, i, nfds); */
                    close(pfds[i].fd);
                    pfds[i].fd = -1;
                    valid_nfds --;
                }
                else if (pfds[i].revents & POLLNVAL) {
                    /* printf("POLLNVAL: %d [%d/%d]\n", pfds[i].fd, i, nfds); */
                    ret = errno;
                    goto kill;
                }
            }
        }

        if (valid_nfds == 0 && flag_wait) {
            if (waitpid(fpid, &status, 0) < 0) {
                ret = errno;
                goto error;
            }
        }

    }
    else { // error
        ret = errno;
        goto error;
    }

    if (info_len > 0) {
        //
        // exec in child process failed, set to errno for reporting
        ret = *cast(int*, info);
    }
    else if (WIFEXITED(status)) {
       *exit_code = WEXITSTATUS(status);
       *pid = fpid;
    }
    else
        goto error;

    goto cleanup;

kill:
    kill(fpid, SIGKILL);
    waitpid(fpid, NULL, 0);

error:
    if (ret == 0)
        ret = -1;

cleanup:
    if (output != NULL && *output != NULL && *output_len <= 0) {
        free(*output);
    }
    if (err != NULL && *err != NULL && *err_len <= 0) {
        free(*err);
    }
    if (info != NULL) {
        free(info);
    }
    if (info_pipe[R] > 0) {
        close(info_pipe[R]);
    }
    if (info_pipe[W] > 0) {
        close(info_pipe[W]);
    }

info_pipe_err:
    if (stderr_pipe[R] > 0) {
        close(stderr_pipe[R]);
    }
    if (stderr_pipe[W] > 0) {
        close(stderr_pipe[W]);
    }

    goto stderr_pipe_err; // no jumps here yet, avoid warning

stderr_pipe_err:
    if (stdout_pipe[R] > 0) {
        close(stdout_pipe[R]);
    }
    if (stdout_pipe[W] > 0) {
        close(stdout_pipe[W]);
    }

stdout_pipe_err:
    if (stdin_pipe[R] > 0) {
        close(stdin_pipe[R]);
    }
    if (stdin_pipe[W] > 0) {
        close(stdin_pipe[W]);
    }

stdin_pipe_err:
    //
    // We will get to this point on success, as well as error (so ret may
    // be 0.  This is the return value of the host kit function to Rebol, not
    // the process exit code (that's written into the pointer arg 'exit_code')
    //
    return ret;
}

#endif


//
//  call: native/export [
//
//  "Run another program; return immediately (unless /WAIT)."
//
//      command [string! block! file!]
//          {An OS-local command line (quoted as necessary), a block with
//          arguments, or an executable file}
//      /wait
//          "Wait for command to terminate before returning"
//      /console
//          "Runs command with I/O redirected to console"
//      /shell
//          "Forces command to be run from shell"
//      /info
//          "Returns process information object"
//      /input
//          "Redirects stdin to in"
//      in [string! binary! file! blank!]
//      /output
//          "Redirects stdout to out"
//      out [string! binary! file! blank!]
//      /error
//          "Redirects stderr to err"
//      err [string! binary! file! blank!]
//  ]
//
REBNATIVE(call)
//
// !!! Parameter usage may require WAIT mode even if not explicitly requested.
// /WAIT should be default, with /ASYNC (or otherwise) as exception!
{
    INCLUDE_PARAMS_OF_CALL;

    UNUSED(REF(shell)); // looked at via frame_ by OS_Create_Process
    UNUSED(REF(console));  // same

    // SECURE was never actually done for R3-Alpha
    //
    Check_Security(Canon(SYM_CALL), POL_EXEC, ARG(command));

    // If input_ser is set, it will be both managed and guarded
    //
    REBSER *input_ser;
    char *os_input;
    REBCNT input_len;

    UNUSED(REF(input)); // implicit by void ARG(in)
    switch (VAL_TYPE(ARG(in))) {
    case REB_STRING:
        input_ser = NULL;
        os_input = cast(char*, Val_Str_To_OS_Managed(&input_ser, ARG(in)));
        PUSH_GUARD_SERIES(input_ser);
        input_len = VAL_LEN_AT(ARG(in));
        break;

    case REB_BINARY:
        input_ser = NULL;
        os_input = s_cast(VAL_BIN_AT(ARG(in)));
        input_len = VAL_LEN_AT(ARG(in));
        break;
        
    case REB_FILE:
        input_ser = Value_To_OS_Path(ARG(in), FALSE);
        MANAGE_SERIES(input_ser);
        PUSH_GUARD_SERIES(input_ser);
        os_input = SER_HEAD(char, input_ser);
        input_len = SER_LEN(input_ser);
        break;

    case REB_BLANK:
    case REB_MAX_VOID:
        input_ser = NULL;
        os_input = NULL;
        input_len = 0;
        break;

    default:
        panic(ARG(in));
    }

    UNUSED(REF(output));
    UNUSED(REF(error));

    REBOOL flag_wait;
    if (
        REF(wait) ||
        (
            IS_STRING(ARG(in)) || IS_BINARY(ARG(in))
            || IS_STRING(ARG(out)) || IS_BINARY(ARG(out))
            || IS_STRING(ARG(err)) || IS_BINARY(ARG(err))
        ) // I/O redirection implies /WAIT
    ){
        flag_wait = TRUE;
    }
    else
        flag_wait = FALSE;
        
    // We synthesize the argc and argv from the "command", and in the
    // process we may need to do dynamic allocations of argc strings.  In
    // Rebol this is always done by making a series, and if those series
    // are managed then we need to keep them SAVEd from the GC for the
    // duration they will be used.  Due to an artifact of the current
    // implementation, FILE! and STRING! turned into OS-compatible character
    // representations must be managed...so we need to save them over
    // the duration of the call.  We hold the pointers to remember to unsave.
    //
    int argc;
    const REBCHR **argv;
    REBCHR *cmd;
    REBSER *argv_ser;
    REBSER *argv_saved_sers;
    REBSER *cmd_ser;

    if (IS_STRING(ARG(command))) {
        // `call {foo bar}` => execute %"foo bar"

        // !!! Interpreting string case as an invocation of %foo with argument
        // "bar" has been requested and seems more suitable.  Question is
        // whether it should go through the shell parsing to do so.

        cmd = Val_Str_To_OS_Managed(&cmd_ser, ARG(command));
        PUSH_GUARD_SERIES(cmd_ser);

        argc = 1;
        argv_ser = Make_Series(argc + 1, sizeof(REBCHR*));
        argv_saved_sers = NULL;
        argv = SER_HEAD(const REBCHR*, argv_ser);

        argv[0] = cmd;
        // Already implicitly SAVEd by cmd_ser, no need for argv_saved_sers

        argv[argc] = NULL;
    }
    else if (IS_BLOCK(ARG(command))) {
        // `call ["foo" "bar"]` => execute %foo with arg "bar"

        cmd = NULL;
        cmd_ser = NULL;

        REBVAL *block = ARG(command);

        argc = VAL_LEN_AT(block);

        if (argc <= 0)
            fail (Error_Too_Short_Raw());

        argv_ser = Make_Series(argc + 1, sizeof(REBCHR*));
        argv_saved_sers = Make_Series(argc, sizeof(REBSER*));
        argv = SER_HEAD(const REBCHR*, argv_ser);

        int i;
        for (i = 0; i < argc; i ++) {
            RELVAL *param = VAL_ARRAY_AT_HEAD(block, i);
            if (IS_STRING(param)) {
                REBSER *ser;
                argv[i] = Val_Str_To_OS_Managed(&ser, KNOWN(param));
                PUSH_GUARD_SERIES(ser);
                SER_HEAD(REBSER*, argv_saved_sers)[i] = ser;
            }
            else if (IS_FILE(param)) {
                REBSER *path = Value_To_OS_Path(KNOWN(param), FALSE);
                argv[i] = SER_HEAD(REBCHR, path);

                MANAGE_SERIES(path);
                PUSH_GUARD_SERIES(path);
                SER_HEAD(REBSER*, argv_saved_sers)[i] = path;
            }
            else
                fail (Error_Invalid_Arg_Core(param, VAL_SPECIFIER(block)));
        }
        argv[argc] = NULL;
    }
    else if (IS_FILE(ARG(command))) {
        // `call %"foo bar"` => execute %"foo bar"

        cmd = NULL;
        cmd_ser = NULL;

        argc = 1;
        argv_ser = Make_Series(argc + 1, sizeof(REBCHR*));
        argv_saved_sers = Make_Series(argc, sizeof(REBSER*));

        argv = SER_HEAD(const REBCHR*, argv_ser);

        REBSER *path = Value_To_OS_Path(ARG(command), FALSE);
        argv[0] = SER_HEAD(REBCHR, path);
        MANAGE_SERIES(path);
        PUSH_GUARD_SERIES(path);
        SER_HEAD(REBSER*, argv_saved_sers)[0] = path;

        argv[argc] = NULL;
    }
    else
        fail (ARG(command));

    REBU64 pid;
    int exit_code;

    // If a STRING! or BINARY! is used for the output or error, then that
    // is treated as a request to append the results of the pipe to them.
    //
    // !!! At the moment this is done by having the OS-specific routine
    // pass back a buffer it malloc()s and reallocates to be the size of the
    // full data, which is then appended after the operation is finished.
    // With CALL now an extension where all parts have access to the internal
    // API, it could be added directly to the binary or string as it goes.
    //
    char *os_output;
    REBCNT output_len;
    char *os_err;
    REBCNT err_len;

    REBINT r = OS_Create_Process(
        frame_,
#ifdef TO_WINDOWS
        cast(const wchar_t*, cmd),
        argc,
        cast(const wchar_t**, argv),
#else
        cast(const char*, cmd),
        argc,
        cast(const char**, argv),
#endif
        flag_wait,
        &pid,
        &exit_code,
        os_input,
        input_len,
        IS_STRING(ARG(out)) || IS_BINARY(ARG(out)) ? &os_output : NULL,
        IS_STRING(ARG(out)) || IS_BINARY(ARG(out)) ? &output_len : NULL,
        IS_STRING(ARG(err)) || IS_BINARY(ARG(err)) ? &os_err : NULL,
        IS_STRING(ARG(err)) || IS_BINARY(ARG(err)) ? &err_len : NULL
    );

    // Call may not succeed if r != 0, but we still have to run cleanup
    // before reporting any error...
    //
    if (argv_saved_sers) {
        int i = argc;
        assert(argc > 0);
        do {
            // Count down: must unsave the most recently saved series first!
            DROP_GUARD_SERIES(*SER_AT(REBSER*, argv_saved_sers, i - 1));
            --i;
        } while (i != 0);
        Free_Series(argv_saved_sers);
    }
    if (cmd_ser != NULL)
        DROP_GUARD_SERIES(cmd_ser);
    Free_Series(argv_ser); // Unmanaged, so we can free it

    if (IS_STRING(ARG(out))) {
        if (output_len > 0) {
            // !!! Somewhat inefficient: should there be Append_OS_Str?
            REBSER *ser = Copy_OS_Str(os_output, output_len);
            Append_String(VAL_SERIES(ARG(out)), ser, 0, SER_LEN(ser));
            free(os_output);
            Free_Series(ser);
        }
    }
    else if (IS_BINARY(ARG(out))) {
        if (output_len > 0) {
            Append_Unencoded_Len(VAL_SERIES(ARG(out)), os_output, output_len);
            free(os_output);
        }
    }

    if (IS_STRING(ARG(err))) {
        if (err_len > 0) {
            // !!! Somewhat inefficient: should there be Append_OS_Str?
            REBSER *ser = Copy_OS_Str(os_err, err_len);
            Append_String(VAL_SERIES(ARG(err)), ser, 0, SER_LEN(ser));
            free(os_err);
            Free_Series(ser);
        }
    } else if (IS_BINARY(ARG(err))) {
        if (err_len > 0) {
            Append_Unencoded_Len(VAL_SERIES(ARG(err)), os_err, err_len);
            free(os_err);
        }
    }

    // If we used (and possibly created) a series for input, then that series
    // was managed and saved from GC.  Unsave it now.  Note backwardsness:
    // must unsave the most recently saved series first!!
    //
    if (input_ser != NULL)
        DROP_GUARD_SERIES(input_ser);

    if (REF(info)) {
        REBCTX *info = Alloc_Context(REB_OBJECT, 2);

        Init_Integer(Append_Context(info, NULL, Canon(SYM_ID)), pid);
        if (REF(wait))
            Init_Integer(
                Append_Context(info, NULL, Canon(SYM_EXIT_CODE)),
                exit_code
            );

        Init_Object(D_OUT, info);
        return R_OUT;
    }

    if (r != 0) {
        Make_OS_Error(D_OUT, r);
        fail (Error_Call_Fail_Raw(D_OUT));
    }

    // We may have waited even if they didn't ask us to explicitly, but
    // we only return a process ID if /WAIT was not explicitly used
    //
    if (REF(wait))
        Init_Integer(D_OUT, exit_code);
    else
        Init_Integer(D_OUT, pid);

    return R_OUT;
}


//
//  get-os-browsers: native/export [
//
//  "Ask the OS or registry what command(s) to use for starting a browser."
//
//      return: [block!]
//          {Block of strings, where %1 should be substituted with the string}
//  ]
//
REBNATIVE(get_os_browsers)
//
// !!! Using the %1 convention is not necessarily ideal vs. having some kind
// of more "structural" result, it was just easy because it's how the string
// comes back from the Windows registry.  Review.
{
    INCLUDE_PARAMS_OF_GET_OS_BROWSERS;

    REBDSP dsp_orig = DSP;

#if defined(TO_WINDOWS)

    HKEY key;
    if (
        RegOpenKeyEx(
            HKEY_CLASSES_ROOT,
            L"http\\shell\\open\\command",
            0,
            KEY_READ,
            &key
        ) != ERROR_SUCCESS
    ){
        fail ("Could not open registry key for http\\shell\\open\\command");
    }

    static_assert_c(sizeof(REBUNI) == sizeof(wchar_t));

    DWORD num_bytes = 0; // pass NULL and use 0 for initial length, to query

    DWORD type;
    DWORD flag = RegQueryValueExW(key, L"", 0, &type, NULL, &num_bytes);
    
    if (
        (flag != ERROR_MORE_DATA && flag != ERROR_SUCCESS)
        || num_bytes == 0
        || type != REG_SZ // RegQueryValueExW returns unicode
        || num_bytes % 2 != 0 // byte count should be even for unicode
    ) {
        RegCloseKey(key);
        fail ("Could not read registry key for http\\shell\\open\\command");
    }

    REBCNT len = num_bytes / 2;

    REBSER *ser = Make_Unicode(len);
    flag = RegQueryValueEx(
        key, L"", 0, &type, cast(LPBYTE, UNI_HEAD(ser)), &num_bytes
    );
    RegCloseKey(key);

    if (flag != ERROR_SUCCESS)
        fail ("Could not read registry key for http\\shell\\open\\command");

    while (*UNI_AT(ser, len - 1) == 0) {
        //
        // Don't count terminators; seems the guarantees are a bit fuzzy
        // about whether the string in the registry has one included in the
        // byte count or not.
        //
        --len;
    }
    TERM_UNI_LEN(ser, len);

    DS_PUSH_TRASH;
    Init_String(DS_TOP, ser);

#elif defined(TO_LINUX)

    // Caller should try xdg-open first, then try x-www-browser otherwise
    //
    DS_PUSH_TRASH;
    Init_String(DS_TOP, Make_UTF8_May_Fail("xdg-open %1"));
    DS_PUSH_TRASH;
    Init_String(DS_TOP, Make_UTF8_May_Fail("x-www-browser %1"));

#else // Just try /usr/bin/open on POSIX, OS X, Haiku, etc.

    // Just use /usr/bin/open
    //
    DS_PUSH_TRASH;
    Init_String(DS_TOP, Make_UTF8_May_Fail("/usr/bin/open %1"));

#endif

    Init_Block(D_OUT, Pop_Stack_Values(dsp_orig));
    return R_OUT;
}


//
//  sleep: native [
//
//  "Use system sleep to wait a certain amount of time (doesn't use PORT!s)."
//
//      return: [<opt>]
//      duration [integer! decimal! time!]
//          {Length to sleep (integer and decimal are measuring seconds)}
//
//  ]
//
REBNATIVE(sleep)
//
// !!! This is a temporary workaround for the fact that it is not currently
// possible to do a WAIT on a time from within an AWAKE handler.  A proper
// solution would presumably solve that problem, so two different functions
// would not be needed.
//
// This function was needed by @GrahamChiu, and putting it in the CALL module
// isn't necessarily ideal, but it's better than making the core dependent
// on Sleep() vs. usleep()...and all the relevant includes have been
// established here.
{
    INCLUDE_PARAMS_OF_SLEEP;

    REBCNT msec = Milliseconds_From_Value(ARG(duration));

#ifdef TO_WINDOWS
    Sleep(msec);
#else
    usleep(msec * 1000);
#endif

    return R_VOID;
}


//
//  get-pid: native [
//
//  "Get ID of the process"
//
//      return: [integer!]
//
//  ]
//
static REBNATIVE(get_pid)
{
    INCLUDE_PARAMS_OF_GET_PID;

#ifdef TO_WINDOWS
    Init_Integer(D_OUT, GetCurrentProcessId());
#else
    Init_Integer(D_OUT, getpid());
#endif

    return R_OUT;
}



//
//  get-uid: native [
//
//  "Get real user ID of the process"
//
//      return: [integer!]
//
//  ]
//
static REBNATIVE(get_uid)
{
    INCLUDE_PARAMS_OF_GET_UID;

#ifdef TO_WINDOWS
    UNUSED(frame_);
    fail ("get-uid is not implemented on Windows");
#else
    Init_Integer(D_OUT, getuid());
#endif

    return R_OUT;
}



//
//  get-euid: native [
//
//  "Get effective user ID of the process"
//
//      return: [integer!]
//
//  ]
//
static REBNATIVE(get_euid)
{
    INCLUDE_PARAMS_OF_GET_EUID;

#ifdef TO_WINDOWS
    UNUSED(frame_);
    fail ("get-euid is not implemented on Windows");
#else
    Init_Integer(D_OUT, geteuid());
#endif

    return R_OUT;
}

//
//  get-gid: native [
//
//  "Get real group ID of the process"
//
//      return: [integer!]
//
//  ]
//
static REBNATIVE(get_gid)
{
    INCLUDE_PARAMS_OF_GET_UID;

#ifdef TO_WINDOWS
    UNUSED(frame_);
    fail ("get-gid is not implemented on Windows");
#else
    Init_Integer(D_OUT, getgid());
#endif

    return R_OUT;
}



//
//  get-egid: native [
//
//  "Get effective group ID of the process"
//
//      return: [integer!]
//
//  ]
//
static REBNATIVE(get_egid)
{
    INCLUDE_PARAMS_OF_GET_EUID;

#ifdef TO_WINDOWS
    UNUSED(frame_);
    fail ("get-egid is not implemented on Windows");
#else
    Init_Integer(D_OUT, getegid());
#endif

    return R_OUT;
}



//
//  set-uid: native [
//
//  "Set real user ID of the process"
//
//      return: [<opt>]
//      uid [integer!]
//          {The effective user ID}
//  ]
//  new-errors: [
//      invalid-uid: ["User id is invalid or not supported:" :arg1]
//      permission-denied: ["The process does not have enough permission"]
//      set-uid-failed: ["set-uid failed with error number:" :arg1]
//  ]
//
static REBNATIVE(set_uid)
{
    INCLUDE_PARAMS_OF_SET_UID;

#ifdef TO_WINDOWS
    UNUSED(frame_);
    UNUSED(ARG(uid));
    fail ("set-uid is not implemented on Windows");
#else
    if (setuid(VAL_INT32(ARG(uid))) < 0) {
        switch (errno) {
            case EINVAL:
                fail (Error(RE_EXT_PROCESS_INVALID_UID, ARG(uid), END));
            case EPERM:
                fail (Error(RE_EXT_PROCESS_PERMISSION_DENIED, END));
            default: {
                DECLARE_LOCAL(err);
                Init_Integer(err, errno);
                fail (Error(RE_EXT_PROCESS_SET_UID_FAILED, err, END));
             }
        }
    }
#endif

    return R_VOID;
}



//
//  set-euid: native [
//
//  "Get effective user ID of the process"
//
//      return: [<opt>]
//      euid [integer!]
//          {The effective user ID}
//  ]
//  new-errors: [
//      invalid-euid: ["user id is invalid or not supported:" :arg1]
//      set-euid-failed: ["set-euid failed with error number:" :arg1]
//  ]
//
static REBNATIVE(set_euid)
{
    INCLUDE_PARAMS_OF_SET_EUID;

#ifdef TO_WINDOWS
    UNUSED(frame_);
    UNUSED(ARG(euid));
    fail ("set-euid is not implemented on Windows");
#else
    if (seteuid(VAL_INT32(ARG(euid))) < 0) {
        switch (errno) {
            case EINVAL:
                fail (Error(RE_EXT_PROCESS_INVALID_EUID, ARG(euid), END));
            case EPERM:
                fail (Error(RE_EXT_PROCESS_PERMISSION_DENIED, END));
            default: {
                DECLARE_LOCAL(err);
                Init_Integer(err, errno);
                fail (Error(RE_EXT_PROCESS_SET_EUID_FAILED, err, END));
             }
        }
    }
#endif

    return R_VOID;
}



//
//  set-gid: native [
//
//  "Set real group ID of the process"
//
//      return: [<opt>]
//      gid [integer!]
//          {The effective group ID}
//  ]
//  new-errors: [
//      invalid-gid: ["group id is invalid or not supported:" :arg1]
//      set-gid-failed: ["set-gid failed with error number:" :arg1]
//  ]
//
static REBNATIVE(set_gid)
{
    INCLUDE_PARAMS_OF_SET_GID;

#ifdef TO_WINDOWS
    UNUSED(frame_);
    UNUSED(ARG(gid));
    fail ("set-gid is not implemented on Windows");
#else
    if (setgid(VAL_INT32(ARG(gid))) < 0) {
        switch (errno) {
            case EINVAL:
                fail (Error(RE_EXT_PROCESS_INVALID_GID, ARG(gid), END));
            case EPERM:
                fail (Error(RE_EXT_PROCESS_PERMISSION_DENIED, END));
            default: {
                DECLARE_LOCAL(err);
                Init_Integer(err, errno);
                fail (Error(RE_EXT_PROCESS_SET_GID_FAILED, err, END));
             }
        }
    }
#endif

    return R_VOID;
}



//
//  set-egid: native [
//
//  "Get effective group ID of the process"
//
//      return: [<opt>]
//      egid [integer!]
//          {The effective group ID}
//  ]
//  new-errors: [
//      invalid-egid: ["group id is invalid or not supported:" :arg1]
//      set-egid-failed: ["set-egid failed with error number:" :arg1]
//  ]
//
static REBNATIVE(set_egid)
{
    INCLUDE_PARAMS_OF_SET_EGID;

#ifdef TO_WINDOWS
    UNUSED(frame_);
    UNUSED(ARG(egid));
    fail ("set-egid is not implemented on Windows");
#else
    if (setegid(VAL_INT32(ARG(egid))) < 0) {
        switch (errno) {
            case EINVAL:
                fail (Error(RE_EXT_PROCESS_INVALID_EGID, ARG(egid), END));
            case EPERM:
                fail (Error(RE_EXT_PROCESS_PERMISSION_DENIED, END));
            default: {
                DECLARE_LOCAL(err);
                Init_Integer(err, errno);
                fail (Error(RE_EXT_PROCESS_SET_EGID_FAILED, err, END));
             }
        }
    }
#endif

    return R_VOID;
}

#if !defined(TO_WINDOWS)
static void kill_process(REBINT pid, REBINT signal)
{
    if (kill(pid, signal) < 0) {
        DECLARE_LOCAL(arg1);
        switch (errno) {
            case EINVAL:
                Init_Integer(arg1, signal);
                fail (Error(RE_EXT_PROCESS_INVALID_SIGNAL, arg1, END));
            case EPERM:
                fail (Error(RE_EXT_PROCESS_PERMISSION_DENIED, END));
            case ESRCH:
                Init_Integer(arg1, pid);
                fail (Error(RE_EXT_PROCESS_NO_PROCESS, arg1, END));
            default:
                Init_Integer(arg1, errno);
                fail (Error(RE_EXT_PROCESS_SEND_SIGNAL_FAILED, arg1, END));
        }
    }
}
#endif


//
//  send-signal: native [
//
//  "Send signal to a process"
//
//      return: [<opt>]
//      pid [integer!]
//          {The process ID}
//      signal [integer!]
//          {The signal number}
//  ]
//  new-errors: [
//      invalid-signal: ["An invalid signal is specified:" :arg1]
//      no-process: ["The target process (group) does not exist:" :arg1]
//      send-signal-failed: ["send-signal failed with error number:" :arg1]
//  ]
//
static REBNATIVE(send_signal)
{
    INCLUDE_PARAMS_OF_SEND_SIGNAL;

#ifdef TO_WINDOWS
    UNUSED(frame_);
    UNUSED(ARG(pid));
    UNUSED(ARG(signal));
    fail ("send-signal is not implemented on Windows");
#else
    kill_process(VAL_INT32(ARG(pid)), VAL_INT32(ARG(signal)));
#endif

    return R_VOID;
}



//
//  terminate: native [
//
//  "Terminate a process (not current one)"
//
//      return: [<opt>]
//      pid [integer!]
//          {The process ID}
//  ]
//  new-errors: [
//      terminate-failed: ["terminate failed with error number:" :arg1]
//  ]
//
static REBNATIVE(terminate)
{
    INCLUDE_PARAMS_OF_TERMINATE;

#ifdef TO_WINDOWS
    if (GetCurrentProcessId() == cast(DWORD, VAL_INT32(ARG(pid)))) {
        fail ("Use QUIT or EXIT-REBOL to terminate current process, instead");
    }
    REBINT err = 0;
    HANDLE ph = OpenProcess(PROCESS_TERMINATE, FALSE, VAL_INT32(ARG(pid)));
    if (ph == NULL) {
        err = GetLastError();
        switch (err) {
            case ERROR_ACCESS_DENIED:
                fail (Error(RE_EXT_PROCESS_PERMISSION_DENIED, END));
            case ERROR_INVALID_PARAMETER:
                fail (Error(RE_EXT_PROCESS_NO_PROCESS, ARG(pid), END));
            default: {
                DECLARE_LOCAL(val);
                Init_Integer(val, err);
                fail (Error(RE_EXT_PROCESS_TERMINATE_FAILED, val, END));
             }
        }
    }
    if (TerminateProcess(ph, 0)) {
        CloseHandle(ph);
        return R_VOID;
    }
    err = GetLastError();
    CloseHandle(ph);
    switch (err) {
        case ERROR_INVALID_HANDLE:
            fail (Error(RE_EXT_PROCESS_NO_PROCESS, ARG(pid), END));
        default: {
            DECLARE_LOCAL(val);
            Init_Integer(val, err);
            fail (Error(RE_EXT_PROCESS_TERMINATE_FAILED, val, END));
         }
    }
#else
    if (getpid() == VAL_INT32(ARG(pid))) {
        // signal is not as reliable for this purpose
        // it's caught in host-main.c as to stop the evaluation
        fail ("Use QUIT or EXIT-REBOL to terminate current process, instead");
    }
    kill_process(VAL_INT32(ARG(pid)), SIGTERM);
#endif

    return R_VOID;
}



#include "tmp-mod-process-last.h"