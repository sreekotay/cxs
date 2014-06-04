// =================================================================================================================
// xs_arr.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_ARR_H_
#define _xs_ARR_H_

#include "xs_atomic.h"


// =================================================================================================================
// basic memory manipulation functions
// =================================================================================================================
typedef int(*xs_ptr_compareproc) (const void *, const void *, const void* privateData);

static int      xs_ptr_insert       (const void* dst, int count, int elemSize, 
                                     const void* src, int index, int insCount);
static int      xs_ptr_remove       (const void* dst, int count, int elemSize, 
                                     int index, int delCount);
static int      xs_ptr_binsearch    (const void* basePtr, int count, int es, 
                                     const void* key, xs_ptr_compareproc compareProc, const void* customData);
static int      xs_ptr_qsort        (const void* basePtr, int count, int es, 
                                     xs_ptr_compareproc compareProc, const void* customData);
static void*    xs_recalloc         (void *ptr, int space, int newSpace, int elemSize);


// =================================================================================================================
// xs_array 
// =================================================================================================================
struct xs_arr {
    void*       aData;
    xs_atomic   aCount;
    int         aSpace;
};      

typedef struct  xs_arr  xs_arr;
typedef struct  xs_arr  xs_array;

static int      xs_arr_create_      (xs_array* ar)                                              {memset(ar, 0, sizeof(*ar));return 0;}
static int      xs_arr_destroy_     (xs_array* ar)                                              {if (ar->aData) free(ar->aData); xs_arr_create_(ar);return 0;}
static int      xs_arr_makespace_   (xs_array* ar, int space, int es)                           {void* p=ar->aData; int ns=ar->aSpace; if (space>=ns) p=(void*)xs_recalloc(p,ar->aSpace,ns=(ns*2>space)?ns*2:space,es); 
                                                                                                 if (p) {ar->aData=p; ar->aSpace=ns;} return p==0;}
static int      xs_arr_remove_      (xs_array* ar, int before, int n, int es)                   {if (n<ar->aCount) xs_ptr_remove (ar->aData, ar->aCount, es, before, n); ar->aCount -= n; return 0;}
static int      xs_arr_insert_      (xs_array* ar, int before, const void* data, int n, int es) {int err=(ar->aCount+n>ar->aSpace)?xs_arr_makespace_(ar,ar->aCount+n,es):0; if (err) return err;
                                                                                                 xs_ptr_insert (ar->aData, ar->aCount, es, data, before, n); ar->aCount += n; return 0;}
static void*    xs_arr_add_         (xs_array* ar, const void* data, int n, int es)             {int err=(ar->aCount+n>ar->aSpace)?xs_arr_makespace_(ar,ar->aCount+n,es):0; char* a=((char*)ar->aData)+es*ar->aCount;
                                                                                                 if (err) return 0; if (data) memcpy (a, data, n*es); ar->aCount += n; return a;}
static void*    xs_arr_             (xs_array* ar, int i, int es)                               {return (char*)ar->aData + i*es;}
static void     xs_arr_replace_     (xs_array* ar, void* data, int c, int s)                    {ar->aData=(void*)data; ar->aCount=c; ar->aSpace=s;}//unsafe
static int      xs_arr_ptrinrange_  (xs_array* ar, void* p, int es)                             {char*cp=(char*)p,*ap=(char*)ar->aData; return cp>=ap&&cp<(ap+ar->aSpace*es);}  

#define         xs_arr_create(ar)                                                                xs_arr_create_(&ar)
#define         xs_arr_destroy(ar)                                                               xs_arr_destroy_(&ar)
#define         xs_arr_makespace(T, ar, space)                                                   (ar).aSpace<=space?xs_arr_makespace_(&ar,space,sizeof(T)):0
#define         xs_arr_add(T, ar, ptr, count)                                                    ((T*)xs_arr_add_(&ar,ptr,count,sizeof(T)))
#define         xs_arr_remove(T, ar, before, n)                                                  xs_arr_remove_(&ar,before,n,sizeof(T))
#define         xs_arr_insert(T, ar, before, ptr, n)                                             xs_arr_insert_(&ar,before,ptr,n,sizeof(T))
#define         xs_arr_push(T, ar, v)                                                            do{xs_arr_makespace(T,ar,(ar).aCount    +1);xs_arr(T,ar,(ar).aCount++)=v;}while(0)
#define         xs_arr_pop(T, ar, v)                                                             ((ar).aCount ? ((v=(((T*)((ar).aData))[--(ar).aCount])), 1) : 0)
#define         xs_arr_count(ar)                                                                 ((ar).aCount)
#define         xs_arr_space(ar)                                                                 ((ar).aSpace)
#define         xs_arr_reset(ar)                                                                 (ar).aCount=0
#define         xs_arr_ptr(T, ar)                                                                ((T*)((ar).aData))
#define         xs_arr(T, ar, i)                                                                 (((T*)((ar).aData))[i])
#define         xs_arr_last(T, ar)                                                               (((T*)((ar).aData))[(ar).aCount-1])
#define         xs_arr_ptrinrange(T, ar, p)                                                      xs_arr_ptrinrange_(&ar,(void*)p,sizeof(T))  













// =================================================================================================================
// implementation
// =================================================================================================================
// =================================================================================================================
// xs_recalloc 
// =================================================================================================================
static void* xs_recalloc(void *p, int oc, int r, int se) {
    if (p==0) {
        p=calloc(r*se, 1);
    } else if (r>oc) {
        p=realloc(p,r*se);
        memset (((char*)p)+oc*se, 0, (r-oc)*se);
    }

    return p;
}

// =================================================================================================================
// xs_ptr_insert - requires preallocated memory for insertion  
// =================================================================================================================
static int xs_ptr_insert (const void* dst, int count, int elemSize, 
                          const void* src, int index, int insCount)
{
    char* pos =((char*)dst) + index*elemSize;
    if ((unsigned int)index > (unsigned int)count)      return 1;
    memmove             (pos + insCount*elemSize, pos,  (count-index)*elemSize);
    if (src) memcpy     (pos, src, insCount*elemSize);
    return 0;
}


// =================================================================================================================
// xs_ptr_remove 
// =================================================================================================================
static int xs_ptr_remove (const void* dst, int count, int elemSize, 
                                           int index, int delCount)
{
    char* pos = ((char*)dst) + index*elemSize;
    if ((unsigned int)index >= (unsigned int)count)             return 1;
    if ((unsigned int)(index+delCount) > (unsigned int)count)   delCount = count-index;
    memmove (pos, pos+delCount*elemSize, (count-(index+delCount))*elemSize);
    return 0;
}


// =================================================================================================================
// xs_BinSearch  
// =================================================================================================================
enum {eBinSearchThresh=8}; //hard-coded to 8? test on other systems....

// =================================================================================================================
// Search  
//      >=0 indicated index, 
//      <0 indicates (-index+1) insertion position
// =================================================================================================================
static int xs_ptr_binsearch (const void* basePtr, int count, int es, const void* key, xs_ptr_compareproc compareProc, const void* customData) {
    char* base = (char*)basePtr;
    int i = 0, n = count, j, k;
    void* guess;

    //compare extremes
    if (count==0)   return -1;
    k               = compareProc(key, base,          customData);
    if (k<=0)       return k==0 ? 0 : -1;
    k               = compareProc(key, base+es*(n-1), customData);
    if (k>=0)       return k==0 ? n-1 : -(n+1);
        
    //compare interiors
    while (i<n) {
        //if it's only a few left, just iterate
        if ((n-i)<eBinSearchThresh) {
            for (j=i; j<n; j++) {
                guess           = base + j*es;
                k               = compareProc(key, guess, customData);
                if (k==0)       return j;
                else if (k<0)   {n=j; break;}
            }

            //failed to find it
            return -(n+1);
        }

        //otherwise binary search
        j           = (int)(((unsigned int)i-(unsigned int)n+1)>>1) + i;
        guess       = base + j*es;
        k           = compareProc(key, guess, customData);
        if (k==0)   return j;
        if (k<0)    n=j;
        else        i=j+1;
    }

    //failed to find it
    return -(n+1);
}


// =================================================================================================================
// Qsort  
// =================================================================================================================
static int xs_ptr_qsort(const void *basevoid, int count, int es, xs_ptr_compareproc proc, const void* customData)
{
    #define xs_local_qsort_swap(a,b)        {memcpy(temp,base+(a)*es,es); memcpy(base+(a)*es,base+(b)*es,es); memcpy(base+(b)*es,temp,es);}
    #define xs_local_qsort_comp(a,b)        (*proc) (base+(b)*es, base+(a)*es, customData)
    
    char* base    = (char*)basevoid;
    int bubThresh = eBinSearchThresh;
    int stack[64];          //stack - since qsort splits, this should be large enough forever?
    int si      = 0;        //stack index 
    int start   = 0;        //start address
    int limit   = count;    //past last element
    char temp[1024];        //for swapping
    int i, j, mid;

    while (1) {
        if (limit-start < bubThresh) {
            // ========================
            // bubble sort
            // ========================
            j = start;
            i = start + 1;
            while (i < limit) {
                while (xs_local_qsort_comp (j, j+1) < 0) {
                    xs_local_qsort_swap (j, j+1);
                    if (j == start)     break;
                    j--;
                }

                j = i++;
            }

            //pop stuff off the stack (if there is anything)
            if (si != 0) {        
                limit    = stack[--si];
                start    = stack[--si];
            } else break;
        } else {
            // ========================
            // quick sort
            // ========================
            mid = ((limit-start)>>1) + start;
            xs_local_qsort_swap (mid, start);

            i       = start  + 1;           
            j       = limit - 1;
            
            //turn start to pivot
            if (xs_local_qsort_comp (i, j)  < 0)        xs_local_qsort_swap (i, j);
            if (xs_local_qsort_comp (start, j)  < 0)    xs_local_qsort_swap (start, j);
            if (xs_local_qsort_comp (i, start)  < 0)    xs_local_qsort_swap (i, start);

            //partition
            while (1) {
                do      i++;
                while   ((i <  limit) && (xs_local_qsort_comp (i, start) > 0));

                do      j--;
                while   ((j >= start) && (xs_local_qsort_comp (j, start) < 0));

                //check bounds
                if (i >= limit)             i = limit;
                if (j <  start)             j = start;

                //are we done?
                if (i > j)                  break;        
                xs_local_qsort_swap         (i, j); //swap and continue
            }

            //place pivot correctly
            xs_local_qsort_swap (start, j); 

            //push to stack
            if (j-start > limit-i) {
                //left larger
                stack[si++] = start;
                stack[si++] = j;
                start       = i;
            } else {            
                //right larger
                stack[si++] = i;
                stack[si++] = limit;
                limit       = j; 
            }
        
            if (si >= 64)   {return -1;} //yikes, overflow
        }
    }

    #undef xs_local_qsort_swap
    #undef xs_local_qsort_comp
    return 0;
}

#endif // for entire file
