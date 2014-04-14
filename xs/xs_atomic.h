// =================================================================================================================
// xs_atomic.h - copyright (c) 2006 2014 Sree Kotay - All rights reserverd
// =================================================================================================================
#ifndef _xs_ATOMIC_H_
#define _xs_ATOMIC_H_

// =================================================================================================================
// very basic atomics
// =================================================================================================================
#define xs_atomic_spin(cond)                do{int i=0,cev;while(i++<10240&&(cev=(cond))){    sched_yield();}if(cev)while(cond){    usleep((++i)>>14);}}while(0)
#define xs_atomic_spin_do(cond, act)        do{int i=0,cev;while(i++<10240&&(cev=(cond))){act;sched_yield();}if(cev)while(cond){act;usleep((++i)>>14);}}while(0)

#ifdef WIN32
#define xs_atomic                           volatile long
#define xs_atomic_inc(ptr)                  (InterlockedIncrement (&(ptr))-1)
#define xs_atomic_dec(ptr)                  (InterlockedDecrement (&(ptr))+1)
#define xs_atomic_add(ptr,v)                (InterlockedExchangeAdd (&(ptr),  (v)))
#define xs_atomic_sub(ptr,v)                (InterlockedExchangeAdd (&(ptr), -(v)))
#define xs_atomic_swap(ptr,oval,nval)       (InterlockedCompareExchange (&(ptr),(nval),(oval)))


#elif defined __GNUC__ &&  (__GNUC__>=3)
#define xs_atomic                           volatile long
#define xs_atomic_inc(ptr)                  __sync_fetch_and_add (&(ptr), 1)
#define xs_atomic_dec(ptr)                  __sync_fetch_and_sub (&(ptr), 1)
#define xs_atomic_add(ptr,v)                __sync_fetch_and_add (&(ptr), (v))
#define xs_atomic_sub(ptr,v)                __sync_fetch_and_sub (&(ptr), (v))
#define xs_atomic_swap(ptr,oval,nval)       __sync_val_compare_and_swap (&(ptr),(oval),(nval))


#else
// =================================================================================================================
// see http://wiki.osdev.org/Inline_Assembly
// =================================================================================================================
typedef volatile long xs_atomic;
static long xs_fetch_and_add(volatile long *mem, long _add)
{
    __asm__ __volatile__ ("lock xaddl %0,%1"
            : "=r" (_add), "+m" (*mem)
            : "r" (_add)
            : "memory");
    return _add;
}
static long xs_compare_and_swap(volatile long *mem, long _old, long _new)
{
    register long r;
    __asm__ __volatile__("lock cmpxchgl %2,%1"
            : "=a" (r), "+m" (*mem)
            : "r" (_new), "0" (_old)
            : "memory");
    return r;
}
#define xs_atomic_swap(ptr,oval,nval)        xs_compare_and_swap (&(ptr),oval,nval)
#define xs_atomic_add(ptr,v)                 xs_fetch_and_add    (&(ptr),  v)
#define xs_atomic_sub(ptr,v)                 xs_fetch_and_add    (&(ptr),-(v))
#define xs_atomic_inc(ptr)                   xs_fetch_and_add    (&(ptr), 1)
#define xs_atomic_dec(ptr)                   xs_fetch_and_add    (&(ptr),-1)

#endif


#endif //for entire file