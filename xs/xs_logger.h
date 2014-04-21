// =================================================================================================================
// xs_logger.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_LOGGER_H_
#define _xs_LOGGER_H_


// =================================================================================================================
// function declarations
// =================================================================================================================
typedef struct xs_compress xs_compress;
enum {
    exs_Log_All,
    exs_Log_Trace,
    exs_Log_Debug,
    exs_Log_Info,
    exs_Log_Warn,
    exs_Log_Error,
    exs_Log_Fatal,
    exs_Log_None
};


const char* xs_timestr        (char *buf, size_t buflen, time_t *t); //t is allowed to be NULL -- in which case buf and buflen may be zero also
const char* xs_timestr_now    (void);

int xs_logger_init      (void);
int xs_logger_level     (int newfilelevel,int newecholevel);
int xs_logger           (int level, const char* fmt, ...);
int xs_logger_va        (int level, const char* fmt, va_list ap);
int xs_access           (const char* fmt, ...);
int xs_access_va        (const char* fmt, ...);
int xs_logger_flush     (void);
int xs_logger_destroy   (void);



int xs_logger_trace     (const char* fmt, ...);
int xs_logger_debug     (const char* fmt, ...);
int xs_logger_info      (const char* fmt, ...);
int xs_logger_warn      (const char* fmt, ...);
int xs_logger_error     (const char* fmt, ...);
int xs_logger_fatal     (const char* fmt, ...);


void xs_logger_counter_add      (int w, int a);
void xs_logger_counter_print    ();

#endif //header







// =================================================================================================================
//  implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_LOGGER_IMPL_
#define _xs_LOGGER_IMPL_

#undef _xs_IMPLEMENTATION_
#ifdef WIN32
#include <io.h>
#endif
#include "xs_atomic.h"
#include "xs_queue.h"
#define _xs_IMPLEMENTATION_

#ifndef O_BINARY
#define O_BINARY 0 
#endif

#define _xscounternum   4
xs_atomic _gxscounter[_xscounternum]={0};
void xs_logger_counter_add(int w, int a) {
    if (w>_xscounternum || w<0) return;
    xs_atomic_add(_gxscounter[w], a);
}
void xs_logger_counter_print() {
    int i;
    xs_printf ("xs atomic counters ");
    for (i=0; i<_xscounternum; i++) xs_printf ("[%ld] ", _gxscounter[i]);
    xs_printf ("\n");
}


typedef struct {
    char* data;
    int count, space;
}xs_logdata;

typedef struct xs_log {
    char path[PATH_MAX];
    xs_atomic count, memlock, flushlock, running;
    pthread_mutex_t mutex;
    pthread_t flushth;
    xs_queue q;
    xs_queue qdel;
    int filelevel:8, echolevel:8, ilevel:8;
    xs_arr buffered;
}xs_log;


void xs_log_process(xs_queue* qs, xs_logdata* ldatp, xs_log* log) {
    int f = open(log->path, O_WRONLY|O_APPEND|O_CREAT|O_BINARY, 0644);
    size_t w;
    (void)qs;
    if (f && ldatp->data) {
        w=write (f, ldatp->data, ldatp->count);
    }
    else printf ("[log write failed]\n"); //$$$SREE - [todo: fix error handling]
    close(f);
    (void)w;

    //save it
    if(ldatp->data && log->running) {
        if (xs_queue_push (&log->qdel, ldatp, 0)!=0)
            free(ldatp->data);
    }
}

void xs_log_flush_threadproc(xs_log* log);
int xs_log_create(xs_log* log, const char* path) {
    memset(log, 0, sizeof(xs_log));

    xs_strlcpy (log->path, path, sizeof(log->path));
    /*
    log->f = fopen(path, "a");
    if (log->f==0) return -108;
    fclose(log->f);
    */
    xs_queue_create (&log->q,    sizeof(xs_logdata), 1024, (xs_queue_proc)xs_log_process, log);
    xs_queue_create (&log->qdel, sizeof(xs_logdata), 1024, 0, 0);
    xs_queue_launchthreads (&log->q, 1, 0);
    log->filelevel = exs_Log_Debug;
    log->echolevel = exs_Log_Info;
    log->running = 1;
    xs_arr_create (log->buffered);
    pthread_mutex_init (&log->mutex, 0);
    pthread_create_detached (&log->flushth, (xs_thread_proc)xs_log_flush_threadproc, log);

    return 0;
}

int xs_log_flush(xs_log* log, char needLock) {
    xs_logdata ldata;
    if (0) {
        pthread_mutex_lock (&log->mutex);
        xs_arr_reset(log->buffered);
        pthread_mutex_unlock (&log->mutex);
        return 0;
    }

    if (log->running==0) return 0;
    if (needLock) pthread_mutex_lock (&log->mutex);
    xs_atomic_inc (log->flushlock);
    while (log->memlock) {sched_yield();  printf ("flush write\n");}
    ldata.data = xs_arr_ptr(char, log->buffered);
    ldata.count = xs_arr_count(log->buffered);
    ldata.space = xs_arr_space(log->buffered);
    if (ldata.count) {
        xs_queue_push(&log->q, &ldata, 1);
        if (xs_queue_pop(&log->qdel, &ldata, 0)!=0)
             xs_arr_replace_(&log->buffered, 0, 0, 0);
        else xs_arr_replace_(&log->buffered, ldata.data, 0, ldata.space);
    }
    if (needLock) pthread_mutex_unlock (&log->mutex);
    xs_atomic_dec (log->flushlock);
    return 0;
}


int xs_log_destroy(xs_log* log) {
    xs_log_flush(log, 1);
    if (xs_atomic_swap(log->running, 1, 0)==1) {
        pthread_mutex_lock (&log->mutex);
        pthread_mutex_unlock (&log->mutex);
        xs_queue_destroy(&log->q);
        xs_queue_destroy(&log->qdel);
        pthread_mutex_destroy(&log->mutex);
        memset(log,0,sizeof(*log));
    }
    return 0;
}

void xs_log_flush_threadproc(xs_log* log) {
    int count;
    do {
        count = xs_arr_count(log->buffered);
        sleep(1);
        if (xs_arr_count(log->buffered)==count && log->running)
            xs_log_flush(log, 1);
    } while (log->running);
}

int xs_log_append(xs_log* log, int level, const char* data, int len) {
    char* arrp;
    xs_atomic_inc(log->count);
    if (data==0 || len==0 || log->running==0) return 0; //below the current level, or nothing
    if (level>=log->echolevel) xs_printf ("%.*s", len, data);
    if (level<log->filelevel) return 0;
    pthread_mutex_lock (&log->mutex);
    if (xs_arr_count(log->buffered)>1000000) {
        pthread_mutex_unlock (&log->mutex);
        xs_log_flush(log, 1);
        pthread_mutex_lock (&log->mutex);
    }
#if 0
    arrp = xs_arr_add(char, log->buffered, data, len);
    pthread_mutex_unlock (&log->mutex);
#else
    //while (log->flushlock) {sched_yield(); printf ("flush lock\n");}
    arrp = xs_arr_add(char, log->buffered, 0, len);
    xs_atomic_inc (log->memlock);
    pthread_mutex_unlock (&log->mutex);
    memcpy (arrp, data, len);
    xs_atomic_dec (log->memlock);
#endif
    return 0;
}

int xs_log_append_str(xs_log* log, int level, const char* str) {
    return xs_log_append(log, level, str, strlen(str));
}

static int xs_logp_cb(void* userdata, const char* buf, int len) {return xs_log_append ((xs_log*)userdata, ((xs_log*)userdata)->ilevel, buf, len);}
int xs_log_printf_va (xs_log* log, int level, char* fmt, va_list ap) {
    int result;
    log->ilevel = level;
    result = xs_sprintf_core (xs_logp_cb, (void*)log, 0, 0, fmt, ap);
    return result;
}

int xs_log_printf (xs_log* log, int level, char* fmt, ...) {
    va_list ap;
    int result;
    va_start(ap, fmt);
    result = xs_log_printf_va (log, level, fmt, ap);
    va_end(ap);
    return result;
}

static const char *gxs_month[]      = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const char *gxs_day[]        = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
__thread time_t gxs_curtime         = 0;
__thread char gx_curtime_str[64]    = "";

const char* xs_timestr(char *buf, size_t buflen, time_t *t) { //t is allowed to be NULL -- in which case buf and buflen may be zero also
    struct tm *tm;
    time_t lt;
    if (t==0) {
        if (buf==0)             {buf=gx_curtime_str; buflen=sizeof(gx_curtime_str);}
        lt = time(NULL); 
        if (lt!=gxs_curtime)    {gxs_curtime = lt;  t = &gxs_curtime;} 
        else                    {if (buf!=gx_curtime_str) xs_strlcpy(buf, gx_curtime_str, buflen); return buf;}
    }
    tm  = gmtime(t);
    if (tm != NULL) {
        xs_sprintf(buf, buflen, "%s, %02d %s %04d %02d:%02d:%02d GMT", 
                            gxs_day[tm->tm_wday], tm->tm_mday,
                            gxs_month[tm->tm_mon], tm->tm_year + 1900, 
                            tm->tm_hour, tm->tm_min, tm->tm_sec);
    } else xs_strlcpy(buf, "Thu, 01 Jan 1970 00:00:00 GMT", buflen);
    if (t==&gxs_curtime && buf!=gx_curtime_str)
        xs_strlcpy(gx_curtime_str, buf, sizeof(gx_curtime_str));
    return buf;
}

const char* xs_timestr_now(void) {return xs_timestr(0, 0, 0);}

xs_log gxslog = {0};
int xs_logger_init () {
    xs_log_create(&gxslog, "./.mylog.log");
    return 0;
}
int xs_logger_level (int newfilelevel, int newecholevel) {
    gxslog.filelevel = newfilelevel;
    gxslog.echolevel = newecholevel;
    return 0;
}

int xs_logger_destroy() {
    xs_log_destroy(&gxslog);
    return 0;
}

int xs_logger_flush(){
    xs_log_flush (&gxslog, 1);
    return 0;
}

char* xs_logger_levelstr(int level) {
    switch (level) {
        case exs_Log_Trace:      return "TRACE ";
        case exs_Log_Debug:      return "DEBUG ";
        case exs_Log_Info:       return "INFO  ";
        case exs_Log_Warn:       return "WARN  ";
        case exs_Log_Error:      return "ERROR ";
        case exs_Log_Fatal:      return "FATAL ";
        default:                 return "-     ";
    }
}


int xs_logger_va (int level, const char* fmt, va_list ap) {
    char str[8192];
    int result=0, l=0, sl=sizeof(str);
    if (gxslog.path[0]==0)                                return -1; //not inited
    if (level<gxslog.filelevel && level<gxslog.echolevel) return 0;  //below the current level
    //return 0;
#if 0
    xs_log_printf (&gxslog, level, "[%s] %s ", xs_timestr_now()+5, xs_logger_levelstr(level)); //+5 skips "SUN, "
    result = xs_log_printf_va (&gxslog, level, fmt, ap);
    xs_log_append (&gxslog, level, "\r\n", 2);
#else
    //xs_sprintf (str, "%s %s ", xs_timestr_now()+5, xs_logger_levelstr(level)); //+5 skips "SUN, "
    l += xs_strlappend (str+l, sl-l, "[", 1);
    l += xs_strappend  (str+l, sl-l, xs_timestr_now()+5) - 4;
    l += xs_strlappend (str+l, sl-l, "] ", 2);
    l += xs_strappend  (str+l, sl-l, xs_logger_levelstr(level));
    l += xs_strlappend (str+l, sl-l, " ", 1);
    l += xs_sprintf_va (str+l, sl-l, fmt, ap);
    l += xs_strlappend (str+l, sl-l, "\r\n", 2);
    xs_log_append (&gxslog, level, str, l);
#endif
/*
    xs_log_append_str (&gxslog, level, xs_timestr_now()+5);
    xs_log_append (&gxslog, level, " ", 1);
    xs_log_append_str (&gxslog, level, xs_logger_levelstr(level));
    xs_log_append (&gxslog, level, " ", 1);
    xs_log_append (&gxslog, level, "test", 4);
*/  
/*    result = xs_log_printf_va (&gxslog, level, fmt, ap);
   // xs_log_append (&gxslog, level, "time ", 5);
    //xs_log_append (&gxslog, level, "test", 4);
    xs_log_append (&gxslog, level, "\r\n", 2);
 */   return result;
}

int xs_logger (int level, const char* fmt, ...) {
    int result;
    va_list ap;
    va_start(ap, fmt);
    result = xs_logger_va(level, fmt, ap);
    va_end(ap);
    return result;
}

#define xs_logger_f(name,level) int xs_logger_##name (const char* fmt, ...) {int r;va_list ap; va_start(ap, fmt); r=xs_logger_va(level, fmt, ap);va_end(ap);return r;}

xs_logger_f(trace, exs_Log_Trace);
xs_logger_f(info,  exs_Log_Info);
xs_logger_f(warn,  exs_Log_Warn);
xs_logger_f(debug, exs_Log_Debug);
xs_logger_f(error, exs_Log_Error);
xs_logger_f(fatal, exs_Log_Fatal);


#endif //_xs_LOGGER_IMPL_
#endif //_xs_IMPLEMENTATION_
