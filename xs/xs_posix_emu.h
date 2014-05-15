// =================================================================================================================
// xs_posix_emu.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_POSIX_EMU_H_
#define _xs_POSIX_EMU_H_

#include "xs_utils.h"

#ifndef WIN32
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <netinet/in.h> 
#include <dirent.h>
#define closesocket(a)             close(a)
#ifndef O_BINARY
#define O_BINARY 0 
#endif //O_BINARY
#else
#include <time.h>
#define __func__ __FUNCTION__
#endif

#include <stdio.h>

static char      xs_path_isabsolute  (const char *path);
static void      xs_path_setabsolute (char *path, int pathsize, const char *rootpath, const char* exepath);


// =================================================================================================================
// basic thread emuation
// =================================================================================================================
typedef void* (*xs_thread_proc)(void*);
#define xs_threadhandle            pthread_t
#ifdef _WIN32
#include <io.h>
    #include <windows.h>
    #include <process.h>
    #include <errno.h>
    #define pid_t                  HANDLE
    typedef HANDLE                 pthread_mutex_t;
    typedef HANDLE                 pthread_t;
    typedef HANDLE                 sem_t;          
    typedef int                    socklen_t;
    typedef struct {
        HANDLE signal, broadcast;
    } pthread_cond_t;

#elif 1// __GNUC__>3 || (__GNUC__==3 && __GNUC_MINOR__>3)
    #include <pthread.h>
    #define xs_file_remove          remove
    #define xs_mkdir                mkdir
    #define xs_fopen                fopen
    #define xs_open                 open
#else 
    ////uh..... out'o'luck you are....
#endif

#if _MSC_VER
#define __thread                    _declspec( thread )
#elif __TINYC__
#define __thread                    
#endif


// =================================================================================================================
// Windows emulation of pthread library
// =================================================================================================================
#if defined(_WIN32) && !defined(__SYMBIAN32__) && !defined(_xs_THREAD_H_)
#define _xs_THREAD_H_
typedef struct xs_taarg{
    void *(*proc)(void*);
    void* arg;
}xs_taarg;
void __cdecl xs_threadproc_create_stub (void* arg) {
    xs_taarg* ta = (xs_taarg*)arg;
    (void)(*(ta->proc))(ta->arg);
    free (ta);
}
static int pthread_create_detached (pthread_t* th, void *(*start_routine) (void *), void* arg) {
    xs_taarg* taarg=(xs_taarg*)malloc(sizeof(xs_taarg));
    taarg->proc = start_routine;
    taarg->arg = arg;
    *th=(pthread_t)_beginthread(xs_threadproc_create_stub, 0, taarg);
    //*th=(pthread_t)_beginthread(start_routine, 0, arg);
    return th ?  0 : -1;
}
static int pthread_cancel(pthread_t th) {
    if (th) TerminateThread((HANDLE)th, 0);
    return 0;
}

static int pthread_join(pthread_t th, void **vptr) {
    return WaitForSingleObject (th, INFINITE);
}

static int pthread_mutex_init(pthread_mutex_t *mutex, void *unused) { 
    unused;
    *mutex = CreateMutex (0, 0, 0);
    return *mutex ? 0 : -1;
}

static int sem_init(sem_t *sem, int pshared, unsigned int value) {
    pshared;
    *sem = CreateSemaphore(0,value,((int)((~0u)>>1)),0);
    return *sem ? 0 : -1;
}

static int pthread_cond_init(pthread_cond_t *cv, const void *unused) {
    unused = NULL;
    cv->signal = CreateEvent(NULL, 0, 0, NULL);
    cv->broadcast = CreateEvent(NULL, 1, 0, NULL);
    return cv->signal != NULL && cv->broadcast != NULL ? 0 : -1;
}

static int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *mutex) {
    HANDLE handles[] = {c->signal, c->broadcast};
    ReleaseMutex(*mutex);
    WaitForMultipleObjects(2, handles, 0, INFINITE);
    return WaitForSingleObject(*mutex, INFINITE) == WAIT_OBJECT_0? 0 : -1;
}

static int      pthread_mutex_destroy(pthread_mutex_t *mutex)       {return CloseHandle(*mutex) ? 0 : -1;}
static int      pthread_mutex_lock(pthread_mutex_t *mutex)          {return WaitForSingleObject(*mutex, INFINITE)==WAIT_OBJECT_0 ? 0 : -1;}
static int      pthread_mutex_trylock(pthread_mutex_t *mutex)       {return WaitForSingleObject(*mutex, 0)==WAIT_TIMEOUT ? 16 : 0;}
static int      pthread_mutex_unlock(pthread_mutex_t *mutex)        {return ReleaseMutex(*mutex) ? 0 : -1;}

static int      pthread_cond_signal(pthread_cond_t *c)              {return SetEvent(c->signal) == 0 ? -1 : 0;}
static int      pthread_cond_broadcast(pthread_cond_t *c)           {return PulseEvent(c->broadcast) == 0 ? -1 : 0;}
static int      pthread_cond_destroy(pthread_cond_t *c)             {return CloseHandle(c->signal) && CloseHandle(c->broadcast) ? 0 : -1;}

static int      sem_destroy(sem_t* sem)                             {return CloseHandle(*sem) ? 0 : -1;}
static int      sem_wait(sem_t* sem)                                {return WaitForSingleObject((HANDLE)*sem,INFINITE); }
static int      sem_trywait(sem_t* sem)                             {return ((WaitForSingleObject((HANDLE)*sem,0)==WAIT_OBJECT_0)?0:EAGAIN); }
static int      sem_post(sem_t *sem)                                {return (ReleaseSemaphore((HANDLE)*sem,1,0)?0:ERANGE);}
static int      sem_getvalue(sem_t *sem, int *sval)                 {long v=-1, e=ReleaseSemaphore((HANDLE)*sem,0,&v); if (sval) *sval=v; return e;}

static int      sched_yield()                                       {Sleep(0); return 0;}
static HANDLE   pthread_self()                                      {return GetCurrentThread();}
    

#ifndef sleep
#define sleep(x)                                                    Sleep((x)*1000)
#endif //sleep

#ifndef usleep
#define usleep(x)                                                   Sleep(x) //not quote right - enh.  good enough.
#endif //usleep

#else  //pthread 
#include <pthread.h>
static int pthread_create_detached (pthread_t* th, void *(*start_routine) (void *), void* arg) {
    int ret = pthread_create (th, 0, start_routine, arg);
    if (ret==0) pthread_detach (*th);
    return ret;
}
#endif //pthread 

// =================================================================================================================
// reader-writer - based on http://www.rfc1149.net/blog/2011/01/07/the-third-readers-writers-problem/
// =================================================================================================================
#ifdef WIN32
typedef struct xs_atomic_readwriter pthread_rwlock_t ;
typedef int pthread_rwlockattr_t ;

static void pthread_rwlock_init(pthread_rwlock_t * prw, pthread_rwlockattr_t* dummy);
static void pthread_rwlock_destroy(pthread_rwlock_t * prw);
static void pthread_rwlock_rdlock(pthread_rwlock_t * prw);
static void pthread_rwlock_rdunlock(pthread_rwlock_t * prw);
static void pthread_rwlock_wrlock(pthread_rwlock_t * prw);
static void pthread_rwlock_wrunlock(pthread_rwlock_t * prw);


// =================================================================================================================
// reader-writer implementation - based on http://www.rfc1149.net/blog/2011/01/07/the-third-readers-writers-problem/
// =================================================================================================================
#define xs_Atomic_Mutex                 pthread_mutex_t
#define xs_atomic_Lock_Init(m)          pthread_mutex_init(&m,NULL)
#define xs_atomic_Lock_Destroy(m)       pthread_mutex_destroy(&m)
#define xs_atomic_Lock(m)               pthread_mutex_lock(&m)
#define xs_atomic_Unlock(m)             pthread_mutex_unlock(&m)

struct xs_atomic_readwriter {
    xs_Atomic_Mutex accessMutex, orderMutex, readersMutex; 
    int readers;
};

//#define PTHREAD_RWLOCK_INITIALIZER        _static_init_rwlock()

const pthread_rwlock_t _static_init_rwlock() {
    pthread_rwlock_t rw;
    pthread_rwlock_init (&rw, 0);
    return rw;
}

static void pthread_rwlock_init(pthread_rwlock_t * prw, pthread_rwlockattr_t* dummy) {
    xs_atomic_Lock_Init(prw->accessMutex);
    xs_atomic_Lock_Init(prw->orderMutex);
    xs_atomic_Lock_Init(prw->readersMutex);
    prw->readers=0;
    dummy;
}
static void pthread_rwlock_destroy(pthread_rwlock_t * prw) {
    pthread_rwlock_wrlock(prw);
    pthread_rwlock_wrunlock(prw);
    xs_atomic_Lock_Destroy(prw->accessMutex);
    xs_atomic_Lock_Destroy(prw->orderMutex);
    xs_atomic_Lock_Destroy(prw->readersMutex);
    prw->readers=0;
}
static void pthread_rwlock_rdlock(pthread_rwlock_t * prw) {
    xs_atomic_Lock(prw->orderMutex);
    xs_atomic_Lock(prw->readersMutex);
    if (prw->readers==0) {
        xs_atomic_Lock(prw->accessMutex);
    }
    prw->readers++;
    xs_atomic_Unlock(prw->orderMutex);
    xs_atomic_Unlock(prw->readersMutex);
}
static void pthread_rwlock_rdunlock(pthread_rwlock_t * prw) {
    xs_atomic_Lock(prw->readersMutex);
    prw->readers--;
    if (prw->readers==0) {
        xs_atomic_Unlock(prw->accessMutex);
    }
    xs_atomic_Unlock(prw->readersMutex);
}
static void pthread_rwlock_wrlock(pthread_rwlock_t * prw) {
    xs_atomic_Lock(prw->orderMutex);
    xs_atomic_Lock(prw->accessMutex);
    xs_atomic_Unlock(prw->orderMutex);
}
static void pthread_rwlock_wrunlock(pthread_rwlock_t * prw) {
    xs_atomic_Unlock(prw->accessMutex);
}
#else //WIN32
#define pthread_rwlock_rdunlock     pthread_rwlock_unlock
#define pthread_rwlock_wrunlock     pthread_rwlock_unlock
#endif


// =================================================================================================================
// Windows emulation of basic .so functions
// =================================================================================================================
#if defined WIN32 && !defined (_xs_DL_H_)
#define _xs_DL_H_
#define RTLD_LAZY   0
static HANDLE       dlopen  (const char *dll_name, int d)           {d; return LoadLibraryA(dll_name);}
static void*        dlsym   (HANDLE h, const char* func_name)       {return (void*)GetProcAddress((HMODULE)h, func_name);}
static int          dlclose (HANDLE h)                              {FreeLibrary ((HMODULE)h); return 0;}
#else  //.so
#include <dlfcn.h>
#endif //.so


// =================================================================================================================
// Windows emulation of poll (using select())
// =================================================================================================================
#ifdef WIN32
#ifndef POLLIN
struct pollfd {
  int fd;
  short events;
  short revents;
};
#endif

#ifndef POLLIN
#define POLLIN      0x01
#endif

#ifndef POLLERR
#define POLLPRI     0x02
#define POLLOUT     0x04
#define POLLERR     0x08
#define POLLHUP     0x10
#define POLLNVAL    0x20
#endif

#endif


#if defined WIN32 || defined HAVE_POLL
#define xs_poll poll
#define HAVE_POLL
static int poll(struct pollfd *pfdin, int ntotal, int milliseconds) {
#else
static int xs_poll(struct pollfd *pfdin, int ntotal, int milliseconds) {
#endif // HAVE_POLL
    struct timeval tv;
    fd_set rset, wset, eset;
    int i, result, n=ntotal;
    socklen_t rlen;
    struct pollfd *pfd=pfdin;
    int maxfd = 0;
    
    if (n>FD_SETSIZE) n=FD_SETSIZE;
    tv.tv_sec  = milliseconds / 1000;
    tv.tv_usec = (milliseconds % 1000)*1000;
    FD_ZERO(&rset);  FD_ZERO(&wset);  FD_ZERO(&eset);
    
    for (i=0; i<n; i++) {
        rlen = sizeof(result);
        if (0 && getsockopt (pfd[i].fd, SOL_SOCKET, SO_ERROR, (char*)&result, &rlen)!=0) { // for debugging
             pfd[i].fd = 0;
             continue;
        }
        if (pfd[i].events&POLLIN)  FD_SET((int) pfd[i].fd, &rset);
        if (pfd[i].events&POLLOUT) FD_SET((int) pfd[i].fd, &wset);
        FD_SET((int) pfd[i].fd, &eset);
        pfd[i].revents = 0;
        
        if ((int)pfd[i].fd>maxfd)  maxfd = pfd[i].fd;
    }
    
    if ((result=select(maxfd + 1, &rset, &wset, &eset, &tv)) > 0) {
        for (i=0; i<n; i++) {
            if (FD_ISSET(pfd[i].fd, &rset)) pfd[i].revents |= POLLIN;
            if (FD_ISSET(pfd[i].fd, &eset)) pfd[i].revents |= POLLERR;
            if (FD_ISSET(pfd[i].fd, &wset)) pfd[i].revents |= POLLOUT;
        }
    }
    
     return result;
}


// =================================================================================================================
// Windows emulation of time functions
// from mongoose webserver -- http://code.google.com/p/mongoose/  (plus some other stuff)
//
//   NOTE: derived from the MIT licensed version of mongoose (pre 4.0)
//
//   Copyright (c) 2004-2013 Sergey Lyubka
//   
//   Permission is hereby granted, free of charge, to any person obtaining a copy
//   of this software and associated documentation files (the "Software"), to deal
//   in the Software without restriction, including without limitation the rights
//   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//   copies of the Software, and to permit persons to whom the Software is
//   furnished to do so, subject to the following conditions:
//   
//   The above copyright notice and this permission notice shall be included in
//   all copies or substantial portions of the Software.
//   
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//   THE SOFTWARE.
// =================================================================================================================
#if (defined WIN32 || !defined CLOCK_MONOTONIC) && !defined (_xs_TIMECLOCK_H_)
#define _xs_TIMECLOCK_H_
struct timespec {
    time_t   tv_sec;        /* seconds */
    long     tv_nsec;       /* nanoseconds */
};

typedef int clockid_t;
#define CLOCK_MONOTONIC (1)
#define CLOCK_REALTIME  (2)

#if WIN32
static int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    FILETIME ft;
    ULARGE_INTEGER li;
    char ok = 0;
    double d;
    static double perfcnt_per_sec = 0.0;

    if (tp) {
        if (clk_id == CLOCK_REALTIME) {
            GetSystemTimeAsFileTime(&ft);
            li.LowPart = ft.dwLowDateTime;
            li.HighPart = ft.dwHighDateTime;
            li.QuadPart -= 116444736000000000; /* 1.1.1970 in filedate */
            tp->tv_sec = (time_t)(li.QuadPart / 10000000);
            tp->tv_nsec = (long)(li.QuadPart % 10000000) * 100;
            ok = 1;
        } else if (clk_id == CLOCK_MONOTONIC) {
            if (perfcnt_per_sec==0) {
                QueryPerformanceFrequency((LARGE_INTEGER *) &li);
                perfcnt_per_sec = 1.0 / li.QuadPart;
            }
            if (perfcnt_per_sec!=0) {
                QueryPerformanceCounter((LARGE_INTEGER *) &li);
                d = li.QuadPart * perfcnt_per_sec;
                tp->tv_sec = (time_t)d;
                d -= tp->tv_sec;
                tp->tv_nsec = (long)(d*1.0E9);
                ok = 1;
            }
        }
    }

    return ok ? 0 : -1;
}
#endif


// =================================================================================================================
// Windows emulation of posix directory functions
// from mongoose webserver -- http://code.google.com/p/mongoose/  (plus some other stuff)
//
//   NOTE: derived from the MIT licensed version of mongoose (pre 4.0)
//
//
//   Copyright (c) 2004-2013 Sergey Lyubka
//   
//   Permission is hereby granted, free of charge, to any person obtaining a copy
//   of this software and associated documentation files (the "Software"), to deal
//   in the Software without restriction, including without limitation the rights
//   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//   copies of the Software, and to permit persons to whom the Software is
//   furnished to do so, subject to the following conditions:
//   
//   The above copyright notice and this permission notice shall be included in
//   all copies or substantial portions of the Software.
//   
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//   THE SOFTWARE.
// =================================================================================================================
#if defined (WIN32) && !defined(__SYMBIAN32__) && !defined(xs_POSIX_FILE_H_) 
#define xs_POSIX_FILE_H_
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

struct dirent {
  char d_name[PATH_MAX];
};

typedef struct DIR {
  HANDLE   handle;
  WIN32_FIND_DATAW info;
  struct dirent  result;
} DIR;

// For Windows, change all slashes to backslashes in path names.
static void change_slashes_to_backslashes(char *path) {
  int i;

  for (i = 0; path[i] != '\0'; i++) {
    if (path[i] == '/')
      path[i] = '\\';
    // i > 0 check is to preserve UNC paths, like \\server\file.txt
    if (path[i] == '\\' && i > 0)
      while (path[i + 1] == '\\' || path[i + 1] == '/')
        (void) memmove(path + i + 1,
            path + i + 2, strlen(path + i + 1));
  }
}

// Encode 'path' which is assumed UTF-8 string, into UNICODE string.
// wbuf and wbuf_len is a target buffer and its length.
static void to_unicode(const char *path, wchar_t *wbuf, size_t wbuf_len) {
  char buf[PATH_MAX], buf2[PATH_MAX];

  xs_strlcpy(buf, path, sizeof(buf));
  change_slashes_to_backslashes(buf);

  // Convert to Unicode and back. If doubly-converted string does not
  // match the original, something is fishy, reject.
  memset(wbuf, 0, wbuf_len * sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, (int) wbuf_len);
  WideCharToMultiByte(CP_UTF8, 0, wbuf, (int) wbuf_len, buf2, sizeof(buf2),
                      NULL, NULL);
  if (strcmp(buf, buf2) != 0) {
    wbuf[0] = L'\0';
  }
}

// Implementation of POSIX opendir/closedir/readdir for Windows.
static DIR * opendir(const char *name) {
  DIR *dir = NULL;
  wchar_t wpath[PATH_MAX];
  DWORD attrs;

  if (name == NULL) {
    SetLastError(ERROR_BAD_ARGUMENTS);
  } else if ((dir = (DIR *) malloc(sizeof(*dir))) == NULL) {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
  } else {
    to_unicode(name, wpath, PATH_MAX);
    attrs = GetFileAttributesW(wpath);
    if (attrs != 0xFFFFFFFF &&
        ((attrs & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)) {
      (void) wcscat (wpath, L"\\*");
      dir->handle = FindFirstFileW(wpath, &dir->info);
      dir->result.d_name[0] = '\0';
    } else {
      free(dir);
      dir = NULL;
    }
  }

  return dir;
}

static int closedir(DIR *dir) {
  int result = 0;

  if (dir != NULL) {
    if (dir->handle != INVALID_HANDLE_VALUE)
      result = FindClose(dir->handle) ? 0 : -1;

    free(dir);
  } else {
    result = -1;
    SetLastError(ERROR_BAD_ARGUMENTS);
  }

  return result;
}

static struct dirent *readdir(DIR *dir) {
  struct dirent *result = 0;

  if (dir) {
    if (dir->handle != INVALID_HANDLE_VALUE) {
      result = &dir->result;
      (void) WideCharToMultiByte(CP_UTF8, 0,
          dir->info.cFileName, -1, result->d_name,
          sizeof(result->d_name), NULL, NULL);

      if (!FindNextFileW(dir->handle, &dir->info)) {
        (void) FindClose(dir->handle);
        dir->handle = INVALID_HANDLE_VALUE;
      }

    } else {
      SetLastError(ERROR_FILE_NOT_FOUND);
    }
  } else {
    SetLastError(ERROR_BAD_ARGUMENTS);
  }

  return result;
}

static int xs_file_remove(const char *path) { //can''t call it "remove" --- taken on windows
  wchar_t wpath[PATH_MAX];
  to_unicode(path, wpath, PATH_MAX);
  return DeleteFileW(wpath) ? 0 : -1;
}


static FILE* xs_fopen(const char *path, const char* mode) {
  wchar_t wpath[PATH_MAX], wmode[16];
  to_unicode(path, wpath, PATH_MAX);
  to_unicode(mode, wmode, 16);
  return _wfopen(wpath, wmode);
}

static int xs_open(const char *path, int flags, int mode) {
  wchar_t wpath[PATH_MAX];
  to_unicode(path, wpath, PATH_MAX);
  return _wopen(wpath, flags, mode);
}


static int xs_mkdir(const char *path, int mode) {
  wchar_t wpath[PATH_MAX];
  char buf[PATH_MAX];
  
  (void) mode;
  xs_strlcpy(buf, path, sizeof(buf));
  change_slashes_to_backslashes(buf);
  to_unicode(buf, wpath, PATH_MAX);
  
  return CreateDirectoryW(wpath, NULL) ? 0 : -1;
}
#endif // xs_POSIX_FILE_H_
#endif //_xs_TIMECLOCK_H_




// =================================================================================================================
// Windows emulation of mmap
// =================================================================================================================
#ifdef WIN32
#define PROT_READ	        (1<<0)
#define PROT_WRITE	        (1<<1)
#define MAP_SHARED	        (1<<0)
#define MAP_PRIVATE	        (1<<1)
#define MAP_FIXED	        (1<<2)

#include "xs_types.h"

static void *mmap(void *addr, size_t length, int prot, int flags, int fd, size_t offset) {
	void *p, *start=0; 
    xsint32 winprot = PAGE_READONLY, winaccess = FILE_MAP_READ;
	SECURITY_ATTRIBUTES sec = { sizeof(SECURITY_ATTRIBUTES), (void*)0, FALSE };
	HANDLE fhan = INVALID_HANDLE_VALUE, han;
	
    //check for valid input
    if (fd>0) {
		fhan = (HANDLE)_get_osfhandle(fd);
		if(fhan==INVALID_HANDLE_VALUE) return 0;
	}

	if (flags&MAP_FIXED) {
		start = addr;
		if(start == 0)	return 0;
	}

	if (prot&PROT_WRITE)   {
		const int shared = ((flags&MAP_SHARED )!=0);
		const int priv   = ((flags&MAP_PRIVATE)!=0);
		winprot = PAGE_READWRITE;
		if (shared && priv)  {
            return 0;
		} else if (priv) {
			winprot             = PAGE_WRITECOPY;
			winaccess           = FILE_MAP_COPY;
		} else if (shared) {
			sec.bInheritHandle  = 1;
			winaccess           = FILE_MAP_ALL_ACCESS;
        }
    } 

    //map the file
	han = CreateFileMapping(fhan, &sec, winprot, ((xsuint64)length)>>32, length&0xffffffff, 0);
	if(han==INVALID_HANDLE_VALUE) return 0;

	p = MapViewOfFileEx(han, winaccess, ((xsuint64)length)>>32, offset, length&0xffffffff, start);
	CloseHandle(han);
	return p;
}


static int munmap(void* const start, const size_t len) {
    (void)len;
	return UnmapViewOfFile(start) ? 0 : -1;
}
#endif //WIN32

// =================================================================================================================
// some simple defines that might be missing
// =================================================================================================================
/*
#ifndef PATH_MAX
#define PATH_MAX 2048
#endif
*/


// =================================================================================================================
// file functions
// =================================================================================================================
#ifdef WIN32
#define DIRSEP '\\'
#define xs_path_canonicalize(rel, abs, abs_size) _fullpath((abs), (rel), (abs_size))
#else
#define DIRSEP '/'
#define xs_path_canonicalize(rel, abs, abs_size) realpath((rel), (abs))
#endif

static char xs_path_isabsolute(const char *path) {
#ifdef _WIN32
	return path != NULL && ((path[0] == '\\' && path[1] == '\\') ||      // is a network path ?
           (xs_isalpha(path[0]) && path[1] == ':' && path[2] == '\\'));  // or a drive path?
#else
	return path != NULL && path[0] == '/';                               // unix abs path?
#endif
}

#include "xs_printf.h"

#ifdef WIN32
#include <direct.h>
#endif

static void xs_path_setabsolute(char *path, int pathsize, const char *rootpath, const char* exepath) {
	char abs[PATH_MAX];
	const char *p;
	if (!xs_path_isabsolute(rootpath)) {
		if ((p=strrchr(exepath, DIRSEP))==0) {
			if (getcwd(path, pathsize)==0) path[0]=0; //failure to get working dir?
		} else xs_sprintf(path, pathsize, "%.*s", (int) (p - exepath), exepath);
		
		xs_strlcat(path, "/", pathsize - 1);
		xs_strlcat(path, rootpath, pathsize - 1);
		if (xs_path_canonicalize(path, abs, sizeof(abs)))
			xs_strlcpy (path, abs, pathsize);
	}
}


#endif // for entire file