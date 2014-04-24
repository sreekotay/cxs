// =================================================================================================================
// xs_startup.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_STARTUP_H_
#define _xs_STARTUP_H_

#include "xs_logger.h"

// =================================================================================================================
// function declarations
// =================================================================================================================
typedef int (*xs_signal_cb) (int signal, void* userData);
enum {
	exs_Start_Signal		= 1<<0,
	exs_Start_CrashLog		= 1<<1,
	exs_Start_All			= -1
};

void	xs_startup			(int flags, xs_signal_cb* proc, void* userdata);

#endif //_xs_STARTUP_H_





// =================================================================================================================
//  implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_STARTUP_IMPL_
#define _xs_STARTUP_IMPL_


#ifndef WIN32
#define __cdecl
#endif
xs_signal_cb gsig_cb=0;
void* gsig_data=0;
static void __cdecl xs_signal_handler(int sig_num) {
	static int sexit = 0;
    xs_logger_flush();
    if (sexit) exit(sexit);
    sexit = sig_num;
    xs_logger_fatal ("SIGNAL %d", sig_num);
    if (gsig_cb) gsig_cb(sig_num, gsig_data);
    xs_logger_flush();
}
static void __cdecl xs_terminate_handler(void) {
    xs_logger_fatal ("TERMINATE");
    signal_handler (-3);
    abort();
    xs_logger_flush();
}
static void __cdecl xs_atexit_handler(void) {
    xs_logger_fatal ("EXIT");
    xs_logger_flush();
    sleep(1);
    xs_logger_counter_print();
}

//from: http://stackoverflow.com/questions/10114711/log-the-reason-for-process-termination-with-c-on-linux
#ifndef WIN32
#include <execinfo.h>
#endif
static void xs_log_stack_trace() {
#ifndef WIN32
    void *arr[256];
    size_t frames = backtrace(arr, sizeof(arr)), i;
    
    char **strings = backtrace_symbols(arr, frames);
    if (strings) {
        xs_logger_fatal("log_stack_trace: begin [%zd frames]", frames);
        for (i=0; i<frames; i++) xs_logger_fatal("  log_stack %s", strings[i]);
        xs_logger_fatal("log_stack_trace: end");
        free(strings);
    }
    else 
#endif
         xs_logger_fatal("log_stack_trace: error - unable to generate stack trace.");
}

static void xs_crash_handler(int sig) {
    // Uninstall this handler, to avoid the possibility of an infinite regress
    signal(SIGSEGV, SIG_DFL);
    signal(SIGILL,  SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGFPE,  SIG_DFL);
#ifndef WIN32
    signal(SIGBUS,  SIG_DFL);
#endif

    xs_logger_fatal("crash_handler signal %i... ", sig);
    xs_log_stack_trace();
    xs_logger_fatal("crash_handler aborting.");
    xs_logger_flush();
    abort();
}

static void xs_register_crash_handler(void) {
	signal(SIGTERM, xs_signal_handler);
	signal(SIGINT,  xs_signal_handler);
    signal(SIGSEGV, xs_crash_handler);
    signal(SIGILL,  xs_crash_handler);
    signal(SIGABRT, xs_crash_handler);
    signal(SIGFPE,  xs_crash_handler);
#ifndef WIN32
    signal(SIGBUS,  crash_handler);
    signal(SIGPIPE, SIG_IGN);
#endif
}


void xs_startup(int flags, xs_signal_cb* proc, void* data) {
    gsig_cb = proc;
	gsig_data = data;
    xs_logger_init();
    xs_register_crash_handler();
    atexit (xs_atexit_handler);
}


#endif //_xs_STARTUP_IMPL_
#endif //_xs_IMPLEMENTATION_