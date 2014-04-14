// =================================================================================================================
// xs_queue.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _XS_QUEUE_H_
#define _XS_QUEUE_H_

#include "xs_atomic.h"
#include "xs_posix_emu.h"

// =================================================================================================================
// function definitions
// =================================================================================================================
typedef void (*xs_queue_proc)(void* qs, void *queueElem, void* privateData);

// =================================================================================================================
// internal structure 
// =================================================================================================================
typedef struct xs_queue xs_queue;
struct xs_queue {
    //core data
    int                 qSpace;
    void*               qData;
    xs_atomic           qWriteInd, qReadInd, qAvailInd, qReadLock, qWriteLock;
    int                 qElemSize;
    xs_atomic           qDepth;
    xs_queue_proc       qProc;
    void*               qProcData;

    //housekeeping
    pthread_mutex_t     mMutex;
    pthread_cond_t      mReady;
    xs_atomic           qDoneCount, qWaitExecCount, qSleepCount, qWakeCount, qOneCount;
    xs_atomic           qIsRunning, qProcessed;
    xs_atomic           qInactive, qRealSleeping, qWorking;
    xs_atomic           qActive;
    xs_threadhandle     watchdogThread;

};

// =================================================================================================================
// functions
// =================================================================================================================
void    xs_queue_create             (xs_queue* q, int elemSize, int queueSize, xs_queue_proc qProc, void* qProcData);
void    xs_queue_destroy            (xs_queue* q);
int     xs_queue_size               (xs_queue* q);
void    xs_queue_add                (xs_queue* q, void* data);
int     xs_queue_push               (xs_queue* q, void* data, char blocking);
int     xs_queue_pop                (xs_queue* q, void* data, char blocking);
void    xs_queue_launchthreads      (xs_queue* q, int count, int flags);
void    xs_queue_launchwatchdog     (xs_queue* q, time_t mytimeout);



#endif //_XS_QUEUE_H_







// =================================================================================================================
// implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_QUEUE_IMPL_
#define _xs_QUEUE_IMPL_


#define xs_queue_index(q,a)             ((a)%(q)->qSpace)


static void xs_queue_create2(xs_queue* qs, int elemSize, int qsize) {
    qs->qWriteInd = qs->qReadInd = qs->qAvailInd = 0;
    qs->qData = elemSize*qsize ? malloc(elemSize*qsize) : 0;
    qs->qSpace = qsize;
    qs->qElemSize = elemSize;
} 
static void xs_queue_destroy2(struct xs_queue* qs) {
    if (qs->qData) free(qs->qData);
    memset(qs, 0, sizeof(*qs));
}



void xs_queue_create(xs_queue* qs, int elemSize, int queueSize, xs_queue_proc qProc, void* qProcData) {
    memset ((void*)qs, 0, sizeof(*qs));
    pthread_cond_init  (&qs->mReady, NULL);
    pthread_mutex_init (&qs->mMutex, NULL);

    pthread_mutex_lock  (&qs->mMutex);

    xs_queue_create2(qs, elemSize, queueSize);

    qs->qProc = qProc;
    qs->qProcData = qProcData;
    qs->qElemSize = elemSize;
    pthread_mutex_unlock(&qs->mMutex);

}

void xs_queue_destroy(xs_queue* qs) {
    qs->qIsRunning=0;
    while (qs->qActive || qs->qInactive)    {pthread_cond_broadcast (&qs->mReady); sched_yield();}
    pthread_mutex_lock      (&qs->mMutex);
    pthread_mutex_unlock    (&qs->mMutex);
    pthread_mutex_destroy   (&qs->mMutex);
    pthread_cond_destroy    (&qs->mReady);
    xs_queue_destroy2 (qs);
}


static void xs_queue_pause (xs_queue *qs) {
    xs_atomic_inc           (qs->qSleepCount);
    pthread_mutex_lock      (&qs->mMutex);
    //printf                ("sleep [%ld]\n", pthread_self());
    xs_atomic_inc           (qs->qInactive);
    xs_atomic_inc           (qs->qRealSleeping);
    //if (qs->qActive-qs->qInactive>(qs->qDepth))
    if (qs->qActive-qs->qInactive-qs->qWorking>(qs->qDepth) || qs->qDepth==0) {
        //printf            ("sleep [%ld]\n", pthread_self());
        pthread_cond_wait   (&qs->mReady, &qs->mMutex);  //nothing to do
        //printf            ("wake [%ld]\n", pthread_self());
    }   
    xs_atomic_dec           (qs->qInactive);
    xs_atomic_dec           (qs->qRealSleeping);
    //printf                ("wake [%ld]\n", pthread_self());
    pthread_mutex_unlock    (&qs->mMutex);
}


static void xs_queue_resume (xs_queue *qs) {
    if (qs->qInactive==0)   return;
    xs_atomic_inc           (qs->qWakeCount);
    pthread_mutex_lock      (&qs->mMutex);
    pthread_cond_signal     (&qs->mReady);
    pthread_mutex_unlock    (&qs->mMutex);
}

void xs_queue_add (xs_queue* qs, void* data) {
    if (qs->qIsRunning==0) return;
    if (qs->qData)  {xs_queue_push(qs,data,1); return;}
}

static void* xs_queue_exec(xs_queue* qs); //forward declaration
void xs_queue_launchthreads (xs_queue* qs, int count, int flags) {
    int i; 
    xs_threadhandle th;
    (void)flags;
    qs->qIsRunning = 1;
    for (i=0; i<count; i++)
        pthread_create_detached (&th, (xs_thread_proc)xs_queue_exec, qs);
}

static void* xs_queue_watchdog_proc (xs_queue* qs) {
    while (qs->qIsRunning) {
        sleep(1);
        printf ("queue fast [0x%lx] p[%ld] d:[%ld] i:[%ld] a:[%ld] w:[%ld] we[%ld]\n",
                (unsigned long)qs, (long)qs->qProcessed, (long)qs->qDepth, 
                (long)qs->qInactive, (long)qs->qActive, (long)qs->qWorking,
                (long)qs->qWaitExecCount
            );
        //if (qs->qInactive && qs->qDepth) xs_queueresume(qs);
    }
    return 0;
}


void xs_queue_launchwatchdog (xs_queue* qs, time_t mytimeout) {
    //qs->watchdog = mytimeout;
    (void)mytimeout;
    pthread_create_detached (&qs->watchdogThread, (xs_thread_proc)xs_queue_watchdog_proc, qs);
}

int xs_queue_size(xs_queue* qs) {
    return qs->qDepth;
}

int xs_queue_grow(xs_queue* qs, int space) {
    void* data;
    size_t si=qs->qElemSize*space;
    if (space==0) space=32;
    xs_atomic_inc (qs->qWriteLock);
    xs_atomic_spin (qs->qReadLock);
    data =  qs->qData ? realloc (qs->qData, si) : malloc(si);
    if (data) {
        qs->qData = data;
        //if (qs->qWriteInd>=qs->qSpace)
    }
    xs_atomic_dec (qs->qWriteLock);
    return 0;
}

int xs_queue_push(xs_queue* qs, void* data, char blocking) {
    int ri, wi, nwi;

#if 0
    if (qs->qDepth>=qs->qSpace) {
        xs_atomic_spin (xs_atomic_swap(qs->qWriteLock),0,1)!=0);
        xs_atomic_spin (qs->qReadLock);
        if (qs->qDepth>=qs->qSpace) xs_queue_grow(qs, qs->qSpace*2);
        xs_atomic_dec (qs->qWriteLock);
    }
    xs_atomic_spin (xs_atomic_swap(qs->qWriteLock),0,1)!=0);
    xs_atomic_inc (qs->qReadLock);
    xs_atomic_dec (qs->qWriteLock);
#endif
    do {
        wi  = qs->qWriteInd;
        ri  = qs->qReadInd; 
        nwi = (wi+1==qs->qSpace) ? 0 : wi+1;
        if (nwi == ri) { //no room
            if (blocking==0) return 1; //busy
            sched_yield();
            continue;
        }
    } while (xs_atomic_swap (qs->qWriteInd, wi, nwi)!=wi);

    //push it
    memcpy (((char*)qs->qData) + wi*qs->qElemSize, data, qs->qElemSize);
    xs_atomic_spin (xs_atomic_swap(qs->qAvailInd, wi, nwi)!=wi);

    //ready for reading
    xs_atomic_inc(qs->qDepth);
    if (qs->qActive) {
        if (qs->qInactive+1==qs->qActive) xs_atomic_inc(qs->qOneCount);
        if (qs->qInactive+1>=qs->qActive) xs_queue_resume(qs); //all sleeping?
        else if (qs->qDepth+qs->qWorking>qs->qActive-qs->qInactive) xs_queue_resume(qs); //all sleeping?
    }

    xs_atomic_dec (qs->qReadLock);
    return 0;
}

int xs_queue_pop(xs_queue* qs, void* data, char blocking) {
    int ri, rmi, nri;
    if (qs->qDepth==0) return -1;
    do {
        ri  = qs->qReadInd; 
        rmi = qs->qAvailInd;
        if (ri == rmi)  //empty
            return -1;

        memcpy (data, ((char*)qs->qData) + ri*qs->qElemSize, qs->qElemSize);

        //done reading
        nri = (ri+1==qs->qSpace) ? 0 : ri+1;
        if (xs_atomic_swap(qs->qReadInd, ri, nri)==ri) {
            xs_atomic_dec(qs->qDepth);
            return 0; //good
        }
    } while(blocking); 

    // busy -- will never exit unless empty or success, if blocking
    return 1;
}

static void* xs_queue_exec (xs_queue* qs) {
    char dd[1024];
    xs_atomic_inc (qs->qActive);
    while (qs->qIsRunning) {
        while (qs->qIsRunning) {
            if (qs->qDepth) {
                if (xs_queue_pop(qs, dd, 0)==0) {
                    xs_atomic_inc (qs->qWorking);
                    if (qs->qProc) qs->qProc ((void*)qs, dd, qs->qProcData);
                    //xs_atomic_dec (qs->qInactive);
                    xs_atomic_inc (qs->qProcessed);
                    xs_atomic_dec (qs->qWorking);
                } else if (qs->qActive-qs->qInactive>(qs->qDepth)) {
                    //printf ("alfq_exec yield\n");
                    xs_atomic_inc (qs->qWaitExecCount); 
                    //sched_yield();//usleep(0);
                }

            } else {
                xs_queue_pause(qs);
            }
        }

        xs_atomic_inc (qs->qDoneCount);
    }
    //printf ("done..... thread.....\n");
    xs_atomic_dec (qs->qActive);
    return 0;
}

// =================================================================================================================
// end internal structure and forward definitions
// =================================================================================================================

#endif //_xs_QUEUE_IMPL_
#endif //_xs_IMPLEMENTATION_