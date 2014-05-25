// =================================================================================================================
// xs_process.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_PROCESS_H_
#define _xs_PROCESS_H_

#include "xs_posix_emu.h"
#include "xs_pipe.h"

struct xs_process_env;

// =================================================================================================================
// function declarations
// =================================================================================================================
static pid_t xs_process_launch( const char *dir, char *prog, const struct xs_process_env* env, int pipein, int pipeout);

#endif //header






// =================================================================================================================
//  implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_PROCESS_IMPL_
#define _xs_PROCESS_IMPL_




int xs_process_closeonexec(int sock) {
#ifdef WIN32
    (void) SetHandleInformation((HANDLE) sock, HANDLE_FLAG_INHERIT, 0);
#else
    fcntl(sock, F_SETFD, FD_CLOEXEC);
#endif
    return 0;
}


typedef struct xs_process_env xs_process_env;
struct xs_process_env {
    xs_arr  data;
};

const char* xs_process_env_data (const xs_process_env* env) {
    return env ? xs_arr_ptr(const char, env->data) : 0;
}

int xs_process_env_add (xs_process_env* env, const char* str) {
    int l = str ? strlen(str) : 0;
#ifdef WIN32
    char empty[] = "\0\0";
    if (l) xs_arr_add  (char, env->data, str, l+1);
    else   xs_arr_add  (char, env->data, empty, 2);
#else
    if (l) xs_arr_push (const char*, env->data, str);
#endif
    return 0;
}




static pid_t xs_process_launch( const char *dir, char *prog, 
                                const xs_process_env* env, 
                                int pipein, int pipeout) {
#ifdef WIN32
    HANDLE curp =  GetCurrentProcess();
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    char interp[]="", fulldir[PATH_MAX], cmdline[PATH_MAX];
    int err;
    
    //setup pipes
    si.cb               = sizeof(si);
    si.dwFlags          = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow      = SW_HIDE;
#ifndef xs_SOCKET_PIPE
    si.hStdInput        = (HANDLE)_get_osfhandle(pipein);
    si.hStdOutput       = (HANDLE)_get_osfhandle(pipeout);
#elif 0
    DuplicateHandle     (curp, (HANDLE) (pipein),  curp, &si.hStdInput,  0, TRUE, DUPLICATE_SAME_ACCESS);
    DuplicateHandle     (curp, (HANDLE) (pipeout), curp, &si.hStdOutput, 0, TRUE, DUPLICATE_SAME_ACCESS);
#elif 1
    si.hStdInput        = (HANDLE)(pipein);
    si.hStdOutput       = (HANDLE)(pipeout);
#else
    DuplicateHandle     (curp, (HANDLE) _get_osfhandle(pipein),  curp, &si.hStdInput,  0, TRUE, DUPLICATE_SAME_ACCESS);
    DuplicateHandle     (curp, (HANDLE) _get_osfhandle(pipeout), curp, &si.hStdOutput, 0, TRUE, DUPLICATE_SAME_ACCESS);
#endif

    //launch process
    GetFullPathNameA (dir ? dir : ".", sizeof(fulldir), fulldir, NULL);
    xs_sprintf (cmdline, sizeof(cmdline), "%s\\%s", fulldir, prog);
    
    if (CreateProcessA(0, cmdline, 0, 0, 1, CREATE_NEW_PROCESS_GROUP, (char*)xs_process_env_data(env), 0, &si, &pi)==0) {
        pi.hProcess = 0;
        err = GetLastError();
        xs_logger_error ("%s: CreateProcess Error ('%s'): %d", __func__, prog, err);
    }
    
    //close "half" the connection
#ifndef xs_SOCKET_PIPE
    if (si.hStdOutput) (void) CloseHandle(si.hStdOutput);
    if (si.hStdInput)  (void) CloseHandle(si.hStdInput);
#endif
    if (pi.hThread)    (void) CloseHandle(pi.hThread);

    //success
    return (pid_t)pi.hProcess;
#else
    pid_t pid;
    const char *ldir=".";
    if (dir==0) dir = ldir;

    //fork
    if ((pid = fork()) == -1) {
        return 0;
    } else if (pid == 0) {
        //launch as child
        if (chdir(dir)!=0)                xs_logger_error ("%s - chdir(%s): %s",   __func__, dir,     strerror(errno));
        else if (dup2(pipein, 0)==-1)     xs_logger_error ("%s - dup2(%d, 0): %s", __func__, pipein,  strerror(errno));
        else if (dup2(pipeout, 1)==-1)    xs_logger_error ("%s - dup2(%d, 1): %s", __func__, pipeout, strerror(errno));
        else {
            close(pipein);
            close(pipeout);
            signal(SIGCHLD, SIG_DFL);
    
            //launch procress
            (void) execle(prog, prog, "webxs.c", NULL, 0);
            xs_logger_error ("%s: execle(%s): %s", __func__, prog,     strerror(errno));
        }

        //done
        exit(EXIT_FAILURE);
    }

    printf ("doing it....\n");

    return pid;

#endif
}

#endif //_xs_PROCESS_IMPL_
#endif //_xs_IMPLEMENTATION_