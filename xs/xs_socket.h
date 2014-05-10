// =================================================================================================================
// xs_socket.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_SOCKET_H_
#define _xs_SOCKET_H_

#ifndef NO_IPV6
#if (!defined(WIN32)&& defined(AF_UNSPEC)) || defined(IPV6_ADDRESS_BITS) //windows is wierd :/
#define USE_IPV6
#endif
#endif //NO_IPV6

#ifdef WIN32
#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif //FD_SETSIZE
#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>
#include <fcntl.h>
#else
#if __TINYC__
#else
//#define FD_SETSIZE 4096
#endif
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif


// =================================================================================================================
//  interface and functions -- declarations
// =================================================================================================================
enum {
    exs_pollfd_New,     //note: userdata for 'exs_pollfd_New' event is actually userdata from the listening socket
    exs_pollfd_Delete,
    exs_pollfd_Read,
    exs_pollfd_Write,
    exs_pollfd_Idle,
    exs_pollfd_Error
};

typedef union xs_sockaddr {
    struct sockaddr         saddr;
    struct sockaddr_in      saddr_in;
    #ifdef USE_IPV6
    struct sockaddr_in6     saddr_in6;  //IPv6
    #endif
} xs_sockaddr;

typedef struct xs_pollfd xs_pollfd;
typedef int (xs_pollfd_Proc) (xs_pollfd* xp, int message, int sockfd, int xptoken, void* userData);

//for use in callback
int             xs_pollfd_clear                 (xs_pollfd* xp, int xptoken);
int             xs_pollfd_setsocket_userdata    (xs_pollfd* xp, int xptoken, void* data);
void*           xs_pollfd_getsocket_userdata    (xs_pollfd* xp, int xptoken);
xs_sockaddr*    xs_pollfd_getsocket_addr        (xs_pollfd* xp, int xptoken);
int             xs_pollfd_setsocket_events      (xs_pollfd* xp, int xptoken, int events); //what events to listen to
int             xs_pollfd_set_userdata          (xs_pollfd* xp, void* data);
void*           xs_pollfd_get_userdata          (xs_pollfd* xp);

//mgmt APIs
xs_pollfd*      xs_pollfd_create                (xs_pollfd_Proc* proc, int* listening, int listenCount);
xs_pollfd*      xs_pollfd_destroy               (xs_pollfd* xp); //you should generally use the refcounting functions below
int             xs_pollfd_run                   (xs_pollfd* xp);
int             xs_pollfd_push                  (xs_pollfd* xp, int sock, void* userdata, char listener);
int             xs_pollfd_setrunning            (xs_pollfd* xp, int state);
int             xs_pollfd_stop                  (xs_pollfd* xp); //convenience -- called by destroy already
xs_pollfd*      xs_pollfd_inc                   (xs_pollfd *xp);
xs_pollfd*      xs_pollfd_dec                   (xs_pollfd *xp);
int             xs_pollfd_print                 (xs_pollfd* xp);
int             xs_pollfd_lock                  (xs_pollfd* xp);

//socket convenience functions
int             xs_sock_open                    (int* sock, const char *host, int port, char use_tcp);
int             xs_sock_listen                  (int* sock, int port, int use_tcp, int ipv6);
void            xs_sock_close                   (int sock);
int             xs_sock_settimeout              (int sock, int milliseconds);
int             xs_sock_setnonblocking          (int sock, unsigned long on);
int             xs_sock_settcpfastopen          (int sockfd);
int             xs_sock_settcpcork              (int sockfd, int on);
int             xs_sock_avail                   (int sock, int event);
int             xs_sock_closeonexec             (int sock);
char*           xs_sock_addrtostr               (char *buf, size_t len, const xs_sockaddr* sa);
char            xs_sock_wouldblock              (int err);
int             xs_sock_err                     ();

#define         xs_sock_valid(a)                (((int)(a))>0)
#define         xs_sock_invalid(a)              (((int)(a))<=0)


// =================================================================================================================
#endif //_xs_pollfd_H_

//helper stuff
#ifndef INVALID_SOCKET
#define INVALID_SOCKET  -1
#endif














// =================================================================================================================
//  implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_SOCKET_IMPL_
#define _xs_SOCKET_IMPL_

#undef _xs_IMPLEMENTATION_
#include <assert.h>
#include <errno.h>
#include "xs_atomic.h"
#include "xs_posix_emu.h"
#include "xs_queue.h"
#include "xs_ssl.h"
#include "xs_logger.h"
#define _xs_IMPLEMENTATION_

#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif

#if (!defined WIN32)&& (!defined HAVE_EPOLL)
//#define HAVE_EPOLL
#endif

#ifdef HAVE_EPOLL
#include <sys/epoll.h>
#endif

struct xs_pollctx;
struct xs_Connfd;


struct xs_socket {
    int                     sock;
    int                     listener, sid;
    xs_sockaddr             sa;
    struct xs_Connfd*       cfd;
};

struct xs_queuesock {   //same as above
    int                     s;
    int                     listener, sid;
    xs_sockaddr             sa;
    void*                   userdata;
};

struct xs_pollfd {
    xs_atomic               refcount;
    xs_threadhandle         th;
    xs_atomic               running;
    int                     listenCount;
    int                     epfd;
    int                     freesid;
    int*                    sidmap;
    struct pollfd*          pfd;
    struct xs_socket*       pss;
    int                     pfdCount, pfdTotal, pfdMax;
    int*                    sockArr;
    xs_atomic               sockCount, sockSpace, sockLock;
    struct xs_pollctx*      ctx;
    xs_pollfd*              root;
    xs_pollfd*              prev;
    xs_pollfd*              next;

    xs_atomic               xpi;
    pthread_mutex_t         pollMutex;
    pthread_cond_t          pollCond;
};



struct xs_pollctx {
    void*               userdata;
    xs_pollfd_Proc*     proc;
    xs_atomic           threadcount;
    xs_atomic           expandlock;
    xs_queue            acceptqueue;
};

// =================================================================================================================
//  internal function definitions
// =================================================================================================================
void*        xs_Pollthread3          (xs_pollfd* xp);
int          xs_pollfd_Init          (xs_pollfd* xp, xs_pollfd* root, int pfdTotal);
int          xs_pollfd_Delete        (xs_pollfd* xp);
xs_pollfd*   xs_pollfd_Find          (xs_pollfd* xp, int pfdTotal);
int          xs_pollfd_Wake          (xs_pollfd* xp, xs_thread_proc proc);
int          xs_pollfd_Clean         (xs_pollfd* inxp);

int          xs_pollfd_Sleep         (xs_pollfd* xp);
char         xs_pollfd_Processing    (xs_pollfd* xp);


// =================================================================================================================
//  external functions implementation
// =================================================================================================================
xs_pollfd*  xs_pollfd_create (xs_pollfd_Proc* proc, int* listening, int listenCount) {
    struct xs_pollfd* xp = (struct xs_pollfd*)calloc(sizeof(struct xs_pollfd), 1);
    int i;
    if (xp==0) return 0;
    xp->ctx = (struct xs_pollctx*)calloc (sizeof(struct xs_pollctx), 1);
    if (xp->ctx==0 || xs_pollfd_Init(xp, 0, listenCount ? listenCount : 0)) {//+ 8 service a few on this thread....? (enh. no.)
        free (xp);
        return 0;
    }
    
    if (listening)
    for (i=0; i<listenCount; i++) {
        xs_sock_setnonblocking(listening[i], 1);
        xs_sock_settcpfastopen(listening[i]);
        xp->pfd[i].fd = listening[i];
        xp->pfd[i].events = POLLIN;
    }

    xp->listenCount = listening ? listenCount : 0;
    xp->pfdCount = xp->listenCount;
    xp->ctx->proc = proc;
    xs_queue_create (&xp->ctx->acceptqueue, sizeof(struct xs_queuesock), 1024, 0, 0);

#ifdef HAVE_EPOLL
    xp->epfd = epoll_create(FD_SETSIZE);//size is ignored
    for (i=0; i<xp->listenCount; i++) {
        struct epoll_event epd = {0};
        epd.events = xp->pfd[i].events;
        epoll_ctl (xp->epfd, EPOLL_CTL_ADD, listening[i], &epd);
    }
#endif
//#undef HAVE_EPOLL
    xs_logger_counter_add (0, 1);
    return xp;
}

xs_pollfd*    xs_pollfd_inc(xs_pollfd *xp) {
    if (xp==0) return 0;
    xs_atomic_inc(xp->refcount);
    return xp;
}

xs_pollfd*    xs_pollfd_dec(xs_pollfd *xp) {
    if (xp==0) return 0;
    if (xs_atomic_dec(xp->refcount)<=1)   return xs_pollfd_destroy(xp);
    return xp;
}

int xs_pollfd_lock(xs_pollfd *xp) {
    if (xp==0) return 0;
    xp->pfdTotal = xp->pfdCount + (xp->ctx?xp->ctx->acceptqueue.qDepth:0);
    return 0;
}

xs_pollfd*  xs_pollfd_destroy (xs_pollfd* xp) {
    struct xs_pollfd *n, *nn;
    xs_pollfd_stop (xp);
    while (xp==xp->root && xp->next && xp->running>=0) {
        xs_pollfd_Wake(xp->next, 0);
        sched_yield();
    }
    if (xp==xp->root) {
        n = xp;
        nn = xp->next;
        while ((n=nn)) {
            n->root = 0;
            nn = n->next;
            if (n!=xp->root) {
                n->ctx = 0;
                xs_pollfd_dec(n);
            }
        }
    }
    if (xp->ctx && (xp->root==0 || xp==xp->root)) {
        if (xp->th) pthread_join (xp->th, 0);
        free (xp->ctx);
        xp->ctx=0;
    }

#ifdef HAVE_EPOLL
    if (xp->epfd)  {close(xp->epfd); xp->epfd=0;}
#endif    
    xs_pollfd_Delete(xp);
    free(xp);
    xs_logger_counter_add (0, -1);
    return 0;
}

int xs_pollfd_run (xs_pollfd* xp) {
    if (xp==0 || xp->ctx==0) return 0;
    xs_Pollthread3 (xp);
    return 0;
}

int xs_pollfd_stop (xs_pollfd* inxp) {
    xs_pollfd* xpr = inxp->root;
    xs_pollfd* xp = xpr ? xpr->next : 0;
    if (xp && xp->running<0) return 0;
    while (xp) {
        if (xp->running>0) {
            while (xs_atomic_swap (xp->running, 1, -2)!=1 &&
                   xs_atomic_swap (xp->running, 2, -2)!=2 &&
                   xs_atomic_swap (xp->running, 3, -2)!=3) { //dead if its "available" or "sleeping"
                sched_yield(); sleep(0); //wait otherwise...
            }
            xp->running = -2;
        }
        xp = xp->next;
    }
    xp = xpr ? xpr->next : 0; 
    while (xp) {
        xs_pollfd_Wake (xp, 0); //kick it
        if (xp->th) pthread_join (xp->th, 0);
        xp = xp->next;
    }
    if (xpr) xpr->running = -2;
    if (xpr) xs_pollfd_Wake (xpr, 0);
    if (xpr && xpr->ctx) {
        xs_atomic_spin(xs_atomic_swap(xpr->ctx->expandlock, 0, 2)==0);
        xs_pollfd_Clean (xpr);
    }
    return 0;
}

int xptok_ind(xs_pollfd* xp, int xptoken) {
    if (xp==0 || xptoken<0 || xptoken>=xp->pfdTotal)    return -1;
    xptoken = xp->sidmap[xptoken];
    if (xptoken<0 || xptoken>=xp->pfdCount)             return -1;
    return xptoken;
}

int xs_pollfd_clear(xs_pollfd* xp, int xptoken) {
    if ((xptoken=xptok_ind(xp,xptoken))<0) return -50;
    xp->pfd[xptoken].fd=0;//INVALID_SOCKET;
    xp->pss[xptoken].cfd=0;
    return 0;
}

int xs_pollfd_setsocket_userdata(xs_pollfd* xp, int xptoken, void* data) {
    if ((xptoken=xptok_ind(xp,xptoken))<0) return -50;
    xp->pss[xptoken].cfd=(struct xs_Connfd*)data;
    return 0;
}

void* xs_pollfd_getsocket_userdata(xs_pollfd* xp, int xptoken) {
    if ((xptoken=xptok_ind(xp,xptoken))<0) return 0;
    return (void*)xp->pss[xptoken].cfd;
}

int xs_pollfd_setsocket_events(xs_pollfd* xp, int xptoken, int events) {
    if ((xptoken=xptok_ind(xp,xptoken))<0) return -50;
    xp->pfd[xptoken].events = events;
    return 0;
}

xs_sockaddr* xs_pollfd_getsocket_addr (xs_pollfd* xp, int xptoken) {
    if ((xptoken=xptok_ind(xp,xptoken))<0) return 0;
    return &xp->pss[xptoken].sa;
}

int xs_pollfd_set_userdata(xs_pollfd* xp, void* data) {
    if (xp==0 || xp->ctx==0) return -50;
    xp->ctx->userdata=(void*)data;
    return 0;
}

void* xs_pollfd_get_userdata(xs_pollfd* xp) {
    if (xp==0 || xp->ctx==0) return 0;
    return xp->ctx->userdata;
}

int xs_pollfd_setrunning(xs_pollfd* xp, int newstate) {
    int s = xp->running;
    if (s>=0) xs_atomic_swap(xp->running, s, newstate);
    return xp->running;
}

int xs_pollfd_push (xs_pollfd* xp, int sock, void* userdata, char listener) {
    struct xs_queuesock qs={0};
    if (xp==0 || xp->ctx==0) return -1;
    qs.s = sock;
    qs.sid = -1;
    qs.listener = listener;
    qs.userdata = userdata;
    xs_queue_push (&xp->ctx->acceptqueue, &qs, 1);
    xs_pollfd_Wake (xp, 0);
    return 0;
}



// =================================================================================================================
//  core function
// =================================================================================================================
void* xs_Pollthread3 (struct xs_pollfd* xp) {
    time_t ct=0, pt=0; 
    xs_queue* qp = &xp->ctx->acceptqueue;
    struct xs_queuesock qs={0};
#ifdef HAVE_EPOLL
    struct epoll_event* eevents = calloc(xp->pfdTotal, 1);
#endif
    int rret = 0;
    xs_pollfd_Proc* cbproc = xp->ctx->proc;
    xs_pollfd_inc (xp);
    xs_atomic_inc (xp->ctx->threadcount);
    xp->running = 1;
    do {
        // ==========================
        // get new from queue
        // ==========================
        while (xp->pfdCount<xp->pfdTotal &&
               xs_queue_pop (qp, &qs, 0)==0) {
            int i, fi;
            if (qs.listener!=0) {
                i = xp->listenCount++;
                memcpy (xp->pfd+i, xp->pfd+xp->pfdMax, sizeof(xp->pfd[i])); //copy to end
            } else i = xp->pfdMax;
            xp->pfdCount++;
            if (xp->freesid<0) {
                assert (i>=0 && i<xp->pfdTotal);
                xp->sidmap[i]   = i;
                xp->pss[i].sid  = fi = i;
                xp->pfdMax++;
            } else {            
                fi              = xp->freesid;
                assert (fi>=0 && fi<xp->pfdTotal);
                xp->freesid     = xp->sidmap[fi];
             #ifdef HAVE_EPOLL
                i               = fi;
                xp->sidmap[fi]  = i;
                xp->pss[i].sid  = fi;
                printf ("over\n");
             #else
                xp->sidmap[fi]  = i;
                xp->pss[i].sid  = fi;
                xp->pfdMax++;
             #endif
           }                   
            xp->pfd[i].events   = POLLIN;
            xp->pfd[i].fd       = qs.s;
            xp->pss[i].sa       = qs.sa;
            xp->pss[i].cfd      = 0;
            xs_pollfd_setrunning(xp, 3);
            (*cbproc) (xp, exs_pollfd_New, qs.s, xp->pss[i].sid, qs.userdata);
            if (qs.listener==0) {}//(*cbproc) (xp, exs_pollfd_New, qs.s, xp->pss[i].sid, qs.userdata);
            else xp->pss[i].cfd = (struct xs_Connfd*)qs.userdata;
        #ifdef HAVE_EPOLL
            if (xp->epfd) {
                struct epoll_event epd = {0};
                epd.events = xp->pfd[i].events;
                epd.data.u32 = fi;
                printf ("add %d: sock[%d]\n", i, xp->pfd[i].fd);
                epoll_ctl (xp->epfd, EPOLL_CTL_ADD, xp->pfd[i].fd, &epd);
            }
        #else
            assert (xp->pfdMax==xp->pfdCount);
        #endif
            xs_pollfd_setrunning(xp, 1);
            rret = 1; //need this if its a "while"
        }

        //launch new thread if necessaryp
        if (xs_atomic_swap (xp->ctx->expandlock, 0, 1)==0) {
            if (qp->qDepth) {// && xp==xp->root) {
                struct xs_pollfd* nxp;
                int fsize = FD_SETSIZE>>3;
                if (xp->root->next && (int)(xp->root->next->pfdTotal*1.61)>fsize) 
                    fsize=(int)(xp->root->next->pfdTotal*1.61);
                if (fsize>FD_SETSIZE) fsize=FD_SETSIZE;
                nxp = xs_pollfd_Find (xp, fsize);
                xs_pollfd_Wake (nxp, (xs_thread_proc)xs_Pollthread3);
            }
            else if (rret==0)// && xp==xp->root)
                xs_pollfd_Clean (xp);
            xp->ctx->expandlock = 0;
        }

        // ==========================
        // poll
        // ==========================
        xs_pollfd_setrunning(xp, 3);
        (void)(*cbproc) (xp, exs_pollfd_Idle, 0, -1, 0);
        xs_pollfd_setrunning(xp, 1);
    #ifdef HAVE_EPOLL
       //printf ("poll %d\n", xp->pfdMax);
       if (xp->pfdCount)
            rret = epoll_wait(xp->epfd, eevents, xp->pfdMax, rret||(qp->qDepth&&xp->pfdCount<xp->pfdTotal) ? 0 : 100 + xp->xpi*5);
        else rret = 0;
    #else
        rret = poll(xp->pfd, xp->pfdCount, (rret>0)||(qp->qDepth&&(!xs_queue_full(qp))&&xp->pfdCount<xp->pfdTotal) ? 0 : 20 + xp->xpi*10);//(/*xp==xp->root*/xp->xpi<4 ? 20 : 200));
        if (0) {//xp->pfdCount>xp->listenCount && 1) { //for debugging
            static time_t ct = 0, pt = 0;
            ct = time(0);
            if (ct+1>pt)
                printf ("rret[%d] = pfd[%d] listen[%d] sockc[%d] queueDepth[%d] xp[0x%lx][%d] tc[%d]\n", rret, 
                xp->pfdCount, (int)xp->listenCount, (int)xp->sockCount, (int)xp->ctx->acceptqueue.qDepth,
                (unsigned long)xp, (int)xp->xpi, (int)xp->ctx->threadcount);
            pt=ct;
        }
    #endif
        if (rret<0) {
            int err = xs_sock_err();
            if (err && xs_sock_wouldblock(err)==0) 
                err=err;
        }
        if (0 && rret && xp->pfdCount>xp->listenCount)
            printf ("rret[%d] = pfd[%d] conncount[%d] qd[%d] xpi[%d] xp[0x%lx][%d] tc[%d]\n", rret, 
                xp->pfdCount, (int)-1002, (int)qp->qDepth, (int)xp->root->xpi,
                (unsigned long)xp, (int)xp->xpi, (int)xp->ctx->threadcount);
        xs_pollfd_setrunning(xp, 1);
        if (rret && xp->running>0) {
        #ifdef HAVE_EPOLL
            int i, rc;
            for (rc=0; rc<rret; rc++) {
                i = xp->sidmap[eevents[rc].data.u32];
                if (xp->pss[i].sid!=i) {printf ("event on closed socket\n"); continue;}

                assert (xp->pss[i].sid==i && i>=0 && i<xp->pfdMax);
                xp->pfd[i].revents = eevents[rc].events;
        #else
            int i, ip;
            for (i=0, ip=0; i<xp->pfdCount; i++) {
        #endif
                int sock = xp->pfd[i].fd;
                assert (xp->sidmap[xp->pss[i].sid]==0 || xp->sidmap[xp->pss[i].sid]==i);
                if (xs_sock_invalid(xp->pfd[i].fd) || xp->pfd[i].revents&(POLLERR|POLLHUP|POLLNVAL)) {
                    // ==========================
                    // error case
                    // ==========================
                    xs_pollfd_setrunning(xp, 3);
                    (void)(*cbproc) (xp, 
                                        xp->pfd[i].revents&(POLLERR|POLLNVAL) ? exs_pollfd_Error : exs_pollfd_Delete, 
                                        xp->pfd[i].fd, xp->pss[i].sid, (void*)(xp->pss[i].cfd));
                    if (i<xp->listenCount) {
                        //error with core socket
                        if (xs_sock_valid(xp->pfd[i].fd)) closesocket(xp->pfd[i].fd);
                        xp->pfd[i].fd  = 0;//INVALID_SOCKET? //already cleaned up
                        xp->pss[i].cfd = 0; 
                        xp->listenCount--;
                    }
                #ifdef HAVE_EPOLL
                    if (0) {
                        struct epoll_event epd = {0}; 
                        printf ("delete\n");
                        epoll_ctl (xp->epfd, EPOLL_CTL_DEL, xp->pfd[i].fd, &epd);
                    }
                #endif
                    xs_pollfd_setrunning(xp, 1);
                } else if (i<xp->listenCount && xp->pfd[i].revents&POLLIN) {
                    // ==========================
                    // new socket
                    // ==========================
                    socklen_t addrlen=sizeof(xp->pss[i].sa), on=1;
                    qs.s = (xp->running>0) ? accept(xp->pfd[i].fd, (struct sockaddr*)&xp->pss[i].sa.saddr_in, &addrlen) : 0;
                    if (qs.s>0) {
                        /*
                        struct linger linger;
                        linger.l_onoff = 1;
                        linger.l_linger = 1;
                        setsockopt(qs.s, SOL_SOCKET, SO_LINGER, (const char *)&linger, sizeof(linger));
                        */
                        xs_sock_setnonblocking(qs.s, 1);
                        #ifdef SO_NOSIGPIPE
                        setsockopt(qs.s, SOL_SOCKET, SO_NOSIGPIPE, (const char*)&on, sizeof(on));
                        #endif
                        setsockopt(qs.s, SOL_SOCKET, SO_KEEPALIVE, (const char*)&on, sizeof(on));
                        setsockopt(qs.s, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
                        xs_sock_settcpfastopen(qs.s);
                        xs_sock_settimeout(qs.s, 1000);

                        qs.sa = xp->pss[i].sa;
                        qs.sid = -1;
                        qs.listener = 0;
                        qs.userdata = xp->pss[i].cfd;
                        xs_queue_push (qp, &qs, 1);
                    } 
                } else {
                    if (xp->pfd[i].revents&POLLIN) {
                        // ==========================
                        // reading
                        // ==========================
                        xs_pollfd_setrunning(xp, 3);
                        (void)(*cbproc) (xp, exs_pollfd_Read, xp->pfd[i].fd, xp->pss[i].sid, (void*)(xp->pss[i].cfd));
                        xs_pollfd_setrunning(xp, 1);

                    } 
                    if (xp->pfd[i].revents&POLLOUT) {
                        // ==========================
                        // writing
                        // ==========================
                        xs_pollfd_setrunning(xp, 3);
                        (void)(*cbproc) (xp, exs_pollfd_Write, xp->pfd[i].fd, xp->pss[i].sid, (void*)(xp->pss[i].cfd));
                        xs_pollfd_setrunning(xp, 1);

                    }
                }

            #ifndef HAVE_EPOLL
                xp->pfd[ip]=xp->pfd[i];
                xp->pss[ip]=xp->pss[i];
                assert (xp->pss[i].sid>=0 && xp->pss[i].sid<xp->pfdTotal);
                if (xs_sock_invalid(xp->pfd[i].fd)) {
                    xp->sidmap[xp->pss[i].sid]      = xp->freesid;
                    xp->freesid                     = xp->pss[i].sid;
                } else xp->sidmap[xp->pss[ip].sid]  = ip;
                ip += xs_sock_valid(xp->pfd[i].fd);
            #else
            #endif
                if (xp->running<0) break;
            }
        #ifndef HAVE_EPOLL
            if (xp->running>0) xp->pfdMax=xp->pfdCount=ip;
        #else
            for (i=0; i<xp->pfdMax; i++) {
                if (xp->pss[i].sid!=i) continue;
                if (xs_sock_invalid(xp->pfd[i].fd)) {
                    printf ("done %d\n", i);
                    xp->sidmap[xp->pss[i].sid]      = xp->freesid;
                    xp->freesid                     = xp->pss[i].sid;
                    xp->pss[i].sid                  = -1;
                    xp->pfdCount--;
                }
            }
        #endif
        }

        while (xp->running>0 && !(xs_pollfd_Processing(xp))) {
            //break;
            xs_pollfd_Sleep(xp);
        }
    } while (xp->running>0 && xs_pollfd_Processing(xp));
    xs_pollfd_setrunning(xp, -1); //dead
    if (xp->ctx) xs_atomic_dec (xp->ctx->threadcount);
#ifdef HAVE_EPOLL
    if (eevents) free(eevents);
#endif
    if (1) {
        int i;
        for (i=0; i<xp->pfdCount; i++) {
            (void)(*cbproc) (xp, exs_pollfd_Delete, xp->pfd[i].fd, xp->pss[i].sid, (void*)(xp->pss[i].cfd));
            //error with core socket
            if (xs_sock_valid(xp->pfd[i].fd)) closesocket(xp->pfd[i].fd);
            xp->pfd[i].fd  = 0;//INVALID_SOCKET? //already cleaned up
            xp->pss[i].cfd = 0; 
        }
        xp->pfdCount = 0;
    }
    //xs_logger_info ("xp thread exit-- xp[%d]: r[%d] pfdt[%d] of [%d]", (int)xp->xpi, (int)xp->running, (int)xp->pfdCount, (int)xp->pfdTotal);
    xs_pollfd_dec (xp);
    return 0;
}


// =================================================================================================================
//  helper functions
// =================================================================================================================
int xs_pollfd_Init(xs_pollfd* xp, xs_pollfd* root, int pfdTotal) {
    if (xp==0) return -50;
    xp->freesid = -1;
    xp->sidmap = (int*)calloc (sizeof(int), pfdTotal);
    xp->pss = (struct xs_socket*)calloc (sizeof(struct xs_socket), pfdTotal);
    xp->pfd = (struct pollfd*)calloc (sizeof(struct pollfd), pfdTotal);
    pthread_mutex_init(&xp->pollMutex, 0);
    pthread_cond_init(&xp->pollCond, 0);
    xp->pfdTotal = pfdTotal;
    if (root) {
        xp->root = root;
        xp->next = root->next;
        if (root->next) root->next->prev = xp;
        xp->prev = root;
        root->next = xp;
        xp->xpi = xs_atomic_inc(root->xpi)+1;
        xp->ctx = root->ctx;
        xs_pollfd_inc (xp);
    } else xp->root = xp;
    xs_logger_info ("xp count:%d size:%d", 
        pfdTotal,
        (int)((sizeof(struct xs_socket)+sizeof(struct pollfd))*pfdTotal + sizeof(*xp))
        );
    return 0;
}

int xs_pollfd_Delete(xs_pollfd* xp) {
    if (xp==0) return -50;
    if (xp->prev)   xp->prev->next=xp->next;
    if (xp->next)   xp->next->prev=xp->prev;
    if (xp->sidmap) free(xp->sidmap);
    if (xp->pfd)    free(xp->pfd);
    if (xp->pss)    free(xp->pss);
    if (xp->root) xs_atomic_dec(xp->root->xpi);
    pthread_mutex_destroy(&xp->pollMutex);
    pthread_cond_destroy(&xp->pollCond);
    memset (xp, 0, sizeof(*xp));
    return 0;
}

int xs_pollfd_Clean(xs_pollfd* inxp) {
    xs_pollfd* root = inxp ? inxp->root : 0, *xp, *t;
    for (xp=root; xp!=0; xp=xp->next) {
        if (inxp==root && xp->running==-1) {
            xs_logger_info ("cleaning...");
            t = xp->prev;
            xs_pollfd_Delete(xp);
            xp = t;
        }
    }
    return 0;
}

int xs_pollfd_print(xs_pollfd* xp) {
    xs_pollfd* root = xp->root;
    xs_logger_info ("thread needed...%d", (int)root->xpi);
    if (1) for (xp=root; xp!=0; xp=xp->next)
        xs_logger_info ("   xp[%d]: r[%d] pfdt[%d] of [%d]", (int)xp->xpi, (int)xp->running, (int)xp->pfdCount, (int)xp->pfdTotal);
    return 0;
}

xs_pollfd* xs_pollfd_Find(xs_pollfd* xp, int pfdTotal) {
    xs_pollfd* root = xp ? xp->root : 0;
    if (xp &&  xp->pfdCount+(xp->pfdCount>>0)+xp->sockCount >= xp->pfdTotal) {
        for (xp=root; xp!=0; xp=xp->next) {
            if (xp->running==2 || 
                (xp->running==1 && (xp->pfdCount+xp->sockCount)<(xp->pfdTotal>>1)) ||
                (xp->running==1 && xp==root && (xp->pfdCount+xp->sockCount)<(xp->pfdTotal>>0)))
                break;
        }
        if (xp==0) // && inxp!=root)
            xs_pollfd_print (root);
    }
    if (xp==0 || (xp->ctx && xs_queue_full (&xp->ctx->acceptqueue))) {
        xp = (struct xs_pollfd*)calloc (1, sizeof(xs_pollfd));
        if (xp) {
            xs_pollfd_Init (xp, root, pfdTotal);
            xs_logger_counter_add (0, 1);
        }
        //printf ("run reason %d\n", 
    }

    return xp;
}

int xs_pollfd_Wake (xs_pollfd* xp, xs_thread_proc proc) {
    int ret=0;
    if (xp==0) return -50;
    if (xp->running==0 && proc) {
        xp->running = 1;
        //printf ("run ind:%d size:%d\n", (int)(xp->xpi), (int)xp->root->xpi);
        //if (nxp->th) XS_endthread (nxp->th);
        if (pthread_create_detached (&xp->th, proc, xp)!=0) {
            xs_logger_info ("------------------- thread error");
            ret=-1;
        }
    } else if (xp->running==2 || xp->running<0) {
        pthread_mutex_lock (&xp->pollMutex);
        pthread_cond_signal (&xp->pollCond);
        pthread_mutex_unlock (&xp->pollMutex);
    }
    return ret;
}

int xs_pollfd_Sleep(xs_pollfd* xp) {
    if (xp==0 || xp->running<=0) return -50;
    pthread_mutex_lock (&xp->pollMutex);
    if (xs_atomic_swap (xp->running, 1, 2)==1) {
        if (xp->pfdCount==0 && xp->listenCount==0 && xp->sockCount==0)
            pthread_cond_wait (&xp->pollCond, &xp->pollMutex);
        xs_atomic_swap (xp->running, 2, 1);
        //if (xp->running>=0) xp->running = 1;
    }
    pthread_mutex_unlock (&xp->pollMutex);
    return 0;
}

char xs_pollfd_Processing(xs_pollfd* xp) {
    if (xp==0) return 0;
    return (xp->pfdCount>xp->listenCount || (/*xp->root==xp &&*/ xp->listenCount) || xp->sockCount || (xp->ctx && xp->ctx->acceptqueue.qDepth /*&& xp->pfdCount<xp->pfdTotal*/));
}



// =================================================================================================================
//  socket helper junk - implementation
// =================================================================================================================
#if defined(_WIN32)
int xs_sock_settimeout(int sock, int milliseconds) {
  DWORD t = milliseconds;
  return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *) &t, sizeof(t)) ||
         setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *) &t, sizeof(t));
}

int  xs_sock_setnonblocking(int sock, unsigned long on) {return ioctlsocket(sock, FIONBIO, &on);}
int  xs_sock_settcpfastopen(int sockfd)                 {return -1;}
int  xs_sock_settcpcork(int sockfd, int on)             {return -1;}
int  xs_sock_err()                                      {return WSAGetLastError();}
char xs_sock_wouldblock(int err)                        {return (err==WSAEWOULDBLOCK);}

#else //_WIN32
int  xs_sock_err()                                      {return errno;}
char xs_sock_wouldblock(int err)                        {return (err==EWOULDBLOCK) || (err==EAGAIN);}
int  xs_sock_settimeout(int sock, int milliseconds) {
  struct timeval t;  t.tv_sec = milliseconds/1000;   t.tv_usec = (milliseconds*1000)%1000000;
  return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *) &t, sizeof(t)) ||
         setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *) &t, sizeof(t));
}

int  xs_sock_setnonblocking(int sock, unsigned long on) {
  int flags = fcntl(sock, F_GETFL, 0);
  if (on)   (void) fcntl(sock, F_SETFL, flags | ( O_NONBLOCK));
  else      (void) fcntl(sock, F_SETFL, flags & (~O_NONBLOCK));
  return 0;
}

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef USE_IPV6
#include <arpa/inet.h>
#endif

#if TCP_FASTOPEN
int xs_sock_settcpfastopen(int sockfd)          {int hint=5; return setsockopt(sockfd, IPPROTO_TCP, TCP_FASTOPEN, &hint, sizeof(hint));}
#else
int xs_sock_settcpfastopen(int sockfd)          {return -1;}
#endif
int xs_sock_settcpcork(int sockfd, int on)      {return setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &on, sizeof(on));}

#endif //_WIN32


int xs_sock_avail (int sock, int event) {
#if 0
    //$$SREEE - fuck - fixme!!! //crashes with concurrency of 10K on ab - switched to poll version below
    int result=0, revents=0;
    fd_set rset, wset, eset;
    struct timeval tv={0};

    FD_ZERO(&rset); FD_ZERO(&wset); FD_ZERO(&eset);
    //return event; //$$SREEE - fuck - fixme!!! //crashes with concurrency of 10K on ab

    if (event&POLLIN)  FD_SET(sock, &rset);
    if (event&POLLOUT) FD_SET(sock, &wset);
    FD_SET(sock, &eset);
    if ((result=select(sock+1, &rset, &wset, &eset, &tv)) > 0) {
        if (FD_ISSET(sock, &rset))  revents |= POLLIN;
        if (FD_ISSET(sock, &eset))  revents |= POLLERR;
        if (FD_ISSET(sock, &wset))  revents |= POLLOUT;
    }

    return revents;
#elif 1
    int result;
    struct pollfd fd;
    fd.fd = sock;
    fd.events = event;
    fd.revents = 0;
    result = poll(&fd,1,0);
    if (result) return fd.revents;
    return 0;
#else
    //NETBSD version -- untested
    int revents=0;
    unsigned long bytes;
    if ((event&POLLIN)  && ioctlsocket (sock, FIONREAD,  &bytes)>0) revents |= POLLIN;
    if ((event&POLLOUT) && ioctlsocket (sock, FIONWRITE, &bytes)>0) revents |= POLLOUT;
    return revents;
#endif
}

int simpnum(char* str, size_t len, int num) {
    int c=0,n=(num>99 ? 3 :(num>9 ? 2 : 1)),r;
    if (n>(int)len) n=len;
    do {r=num; num/=10; r-=num*10; str[n-(++c)]=(char)('0'+r);} while (num);
    return n;
}
int xs_inet_ntoa(char* str, size_t len, const unsigned char* sa4) {
    int n=0;
    n+=simpnum(str+n,len-n,sa4[0]);   str[n++]='.';
    n+=simpnum(str+n,len-n,sa4[1]);   str[n++]='.';
    n+=simpnum(str+n,len-n,sa4[2]);   str[n++]='.';
    n+=simpnum(str+n,len-n,sa4[3]);   str[n++]=0;
    return n;
}
#ifdef USE_IPV6
int xs_inet_ntop(char* str, size_t len, const unsigned char* sa6) {
    int c=0;
    while (sa6[c]==0) c++; //skip leading zeros
    if (c==10 && sa6[c+0]==0xff && sa6[c+1]==0xff) return /*inet_ntop (AF_INET, (void*)(sa6+c+2), str, len);//*/xs_inet_ntoa(str, len, sa6+c+2);
    inet_ntop (AF_INET6, (void*)sa6, str, len);
    return c;
}
#endif

char* xs_sock_addrtostr(char *str, size_t len, const xs_sockaddr *sa) {
    if (str==0) return 0;
    str[0] = 0; //terminate
#ifdef USE_IPV6
    if (sa->saddr.sa_family == AF_INET)         xs_inet_ntoa(str, len, (const unsigned char*)&sa->saddr_in.sin_addr);
    else if (sa->saddr.sa_family == AF_INET6)   xs_inet_ntop(str, len, (const unsigned char*)&sa->saddr_in6.sin6_addr);
    else inet_ntop  (sa->saddr.sa_family, 
                     sa->saddr.sa_family == AF_INET ? (void*)&sa->saddr_in.sin_addr : (void*)&sa->saddr_in6.sin6_addr, str, len);
#elif !defined(WIN32) || (defined(WIN32) && defined (_WS2DEF_) && (NTDDI_VERSION >= NTDDI_VISTA)) 
    inet_ntop (sa->saddr.sa_family, (void*)&sa->saddr_in.sin_addr, str, len);
#else
    xs_inet_ntoa(str, len, (const char*)&sa->saddr_in.sin_addr);
    //xs_strlcpy(str, inet_ntoa(sa->saddr_in.sin_addr), len);
#endif
    return str;
}

void xs_sock_close (int sock) {
    shutdown(sock, 1);
    closesocket(sock);
}

int xs_sock_closeonexec(int sock) {
#ifdef WIN32
    (void) SetHandleInformation((HANDLE) sock, HANDLE_FLAG_INHERIT, 0);
#else
    fcntl(sock, F_SETFD, FD_CLOEXEC);
#endif
    return 0;
}

int xs_sock_open (int* sockp, const char *host, int port, char use_tcp) {
#ifdef USE_IPV6
    struct addrinfo *si=0, hints={0};
    char portstr[64];
    int sock = INVALID_SOCKET, result;
    if (sockp==0 || host==NULL) return -50;

    // get hostinfo and socket ready
    hints.ai_family     = AF_UNSPEC;                        //we want both IPv4 or IPv6
    hints.ai_socktype   = use_tcp?SOCK_STREAM:SOCK_DGRAM;   //stream type?
    xs_itoa(port, portstr, sizeof(portstr), 10);
    if ((result = getaddrinfo(host, portstr, &hints, &si))!=0)                              return result;
    if ((sock = socket(si->ai_family, si->ai_socktype, si->ai_protocol)) == INVALID_SOCKET) return -3;
    
    //connect
    xs_sock_closeonexec(sock);
    if ((result = connect(sock, si->ai_addr, si->ai_addrlen)) != 0) {
        closesocket(sock);
        return result;
    }

    freeaddrinfo(si);
#else
    struct sockaddr_in sin;
    struct hostent *hent;
    int sock = INVALID_SOCKET, result;
    if (sockp==0 || host==NULL) return -50;

    // get hostinfo and socket ready
    if ((hent = gethostbyname(host)) == NULL)                                           return -2;
    if ((sock = socket(PF_INET, use_tcp?SOCK_STREAM:SOCK_DGRAM, 0)) == INVALID_SOCKET)  return -3;
    
    //connect
    xs_sock_closeonexec(sock);
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short) port);
    sin.sin_addr = * (struct in_addr *) hent->h_addr_list[0];
    if ((result=connect(sock, (struct sockaddr*)&sin, sizeof(sin))) != 0) {
        closesocket(sock);
        return result;
    }
#endif

    *sockp = sock;
    return 0;
}

int xs_sock_listen (int* sock, int port, int use_tcp, int ipv6) {
    int sockfd, on=1;
#ifdef USE_IPV6
    int result, off=0;
    struct addrinfo hints={0}, *si=0;
    char portstr[64];

    //use getaddrinfo
    hints.ai_family     = ipv6 ? AF_INET6 : AF_INET;    //we want either IPv4 or IPv6
    hints.ai_socktype   = use_tcp?SOCK_STREAM:SOCK_DGRAM;
    hints.ai_flags      = AI_PASSIVE;    
    xs_itoa(port, portstr, sizeof(portstr), 10);
    if ((result=getaddrinfo(NULL, portstr, &hints, &si))!=0) return result;

    //streaming socket
    sockfd = socket(si->ai_family, si->ai_socktype, si->ai_protocol);
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
    #ifdef SO_NOSIGPIPE
    setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&on, sizeof(on));
    #endif
    if (ipv6 && si->ai_family == AF_INET6)
        setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (const char *) &off, sizeof(off));

    //bind and listen
    if (bind(sockfd, si->ai_addr, si->ai_addrlen) != 0 ||
        listen(sockfd, SOMAXCONN) != 0) {
        return -2;
    }

    freeaddrinfo(si);
#else
    struct sockaddr_in self;

    //streaming socket
    if (ipv6 || (sockfd = socket(AF_INET, use_tcp?SOCK_STREAM:SOCK_DGRAM, 0)) < 0)
        return -1;

    //setup address/port
    memset(&self, 0, sizeof(self));
    self.sin_family = AF_INET;
    self.sin_port = htons(port);
    self.sin_addr.s_addr = INADDR_ANY;

    #ifdef SO_NOSIGPIPE
    setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&on, sizeof(on));
    #endif
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));

    //bind and listen
    if (bind(sockfd, (struct sockaddr*)&self, sizeof(self)) != 0 ||
        listen(sockfd, SOMAXCONN) != 0) {
        return -2;
    }
#endif
    *sock = sockfd;
    return 0;
}
#endif //_xs_pollfd_IMPL_
#endif //_xs_IMPLEMENTATION_
