// =================================================================================================================
// xs_fileinfo.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_FILEINFO_H_
#define _xs_FILEINFO_H_


// =================================================================================================================
// function declarations
// =================================================================================================================
typedef struct xs_fileinfo{
    xs_atomic                   status, readcount;
    time_t                      check_time;
    int                         stat_ret;
    size_t                      size;
    time_t                      modification_time, folder_mod;
    int                         is_directory;
    char*                       data;
    char*                       userdata;
    int                         userinfo;
} xs_fileinfo;


int     xs_stat                 (const char* path, xs_fileinfo* filep);     //portable 'stat' function
int     xs_fileinfo_init        ();
int     xs_fileinfo_get         (xs_fileinfo** fip, const char *path, int load_data);
void    xs_fileinfo_lock        (xs_fileinfo*  fi); //lock function
void    xs_fileinfo_unlock      (xs_fileinfo*  fi); //lock function
int     xs_fileinfo_loaddata    (xs_fileinfo*  fi,  const char *path);

#endif //header





// =================================================================================================================
//  implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_FILEINFO_IMPL_
#define _xs_FILEINFO_IMPL_

#define _use_MMAP_      1

#include "khash.h"

#ifdef WIN32
#else
#include <sys/stat.h>
#include <sys/mman.h>
#endif

#ifdef WIN32
static int xs_properfilename(const char *path) { //windows fopen() may open even if invalid at end of filepath...
  const char *okchars = "_-";
  int endch = path[strlen(path) - 1];
  return xs_isalnum(endch) || strchr(okchars, endch)!=0;
}
#endif

int xs_path_getdirectory(char* dirpath, size_t maxpath, const char* filepath) {
    const char* ch = filepath+strlen(filepath);
    while (ch!=filepath && *ch!='\\' && *ch!='/') ch--;
    maxpath--;//termination
    if (ch==filepath || (size_t)(ch+1-filepath)>maxpath) *dirpath=0;
    else {memcpy (dirpath, filepath, ch+1-filepath); dirpath[ch+1-filepath]=0;}
    return *dirpath ? 0 : -1;
}

int xs_stat(const char* path, xs_fileinfo* filep) {
#ifdef WIN32
    #define MAKE64(lo, hi)          ((xsuint64)(((xsuint32)(lo)) | ((xsuint64)((xsuint32)(hi)))<<32))
    #define EPOCH                   MAKE64(0xd53e8000, 0x019db1de)
    #define RATE                    10000000 
    #define MAKEUNIX_TIME(lo, hi)   (time_t)((MAKE64((lo),(hi))-EPOCH)/RATE)
    WIN32_FILE_ATTRIBUTE_DATA info;
    wchar_t wpath[PATH_MAX];
    if (filep==0) return -1;
    memset (filep, 0, sizeof(*filep));
    to_unicode(path, wpath, PATH_MAX);
    if (GetFileAttributesExW(wpath, GetFileExInfoStandard, &info) != 0) {
        filep->size = MAKE64(info.nFileSizeLow, info.nFileSizeHigh);
        filep->modification_time = MAKEUNIX_TIME(info.ftLastWriteTime.dwLowDateTime, info.ftLastWriteTime.dwHighDateTime);
        filep->is_directory = ((info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0);
        if (filep->is_directory==0 && xs_properfilename(path)==0) {
            memset(filep, 0, sizeof(*filep));
            filep->stat_ret = -1;
        } else filep->stat_ret = 0;
    } else filep->stat_ret = -1;
#else
    struct stat st;
    if (filep==0) return -1;
    memset (filep, 0, sizeof(*filep));
    if (stat(path, &st)==0) {
        filep->stat_ret = 0;
        filep->size = st.st_size;
        filep->modification_time = st.st_mtime;
        filep->is_directory = S_ISDIR(st.st_mode);
    } else {
        memset (filep, 0, sizeof(*filep));
        filep->stat_ret = -1;
    }
#endif
    if (filep->is_directory) filep->size=0;
    return filep->stat_ret;
}

//#define _old_file_stuff_


KHASH_MAP_INIT_STR(statptr, xs_fileinfo*);
kh_statptr_t* gxs_statptr = 0;
xs_atomic gxs_statwrite = 0;
xs_atomic gxs_statread = 0;
pthread_rwlock_t gxs_stat_rwlock;
pthread_mutex_t gxs_statmutex;

int xs_fileinfo_init() {
    if (gxs_statptr) return 0;
    gxs_statptr = kh_init_statptr();
    pthread_mutex_init(&gxs_statmutex, 0);
    pthread_rwlock_init(&gxs_stat_rwlock, 0);
    return 0;
}

void xs_fileinfo_global_lockstatus(int oldstatus, int newstatus) {
   if (newstatus==2) {
        xs_atomic_spin (xs_atomic_swap(gxs_statwrite,0,1)!=0);
        //xs_atomic_inc (gxs_statwrite);
        if (oldstatus==1) xs_atomic_dec (gxs_statread);
        xs_atomic_spin_do(gxs_statread, {});//printf ("yield hash\n"));
        return;
    }
    if (oldstatus==2) {
        xs_atomic_dec (gxs_statwrite);
        if (newstatus==1) xs_atomic_inc (gxs_statread);
        return;
    }
    if (oldstatus==1) {
        xs_atomic_dec (gxs_statread);
        return;
    }
    if (oldstatus==0) {
        xs_atomic_spin_do(gxs_statwrite, {});//printf ("yield file\n"));
        //xs_atomic_spin_do(xs_atomic_swap(gxs_statwrite,0,1)!=0, printf ("yield file\n"));
        //xs_atomic_spin (xs_atomic_swap(gxs_statwrite,0,1)!=0);
        xs_atomic_inc (gxs_statread);
        //xs_atomic_dec (gxs_statwrite);
        return;
    }
}

void xs_fileinfo_lock(xs_fileinfo* fi) {
#ifndef _old_file_stuff_
    xs_atomic_spin_do (fi->readcount==0 && fi->status!=10, printf ("yield file\n"));
    xs_atomic_add (fi->readcount, 1);
#endif
}

void xs_fileinfo_unlock(xs_fileinfo* fi) {
#ifndef _old_file_stuff_
    xs_atomic_spin_do (fi->readcount==0 && fi->status!=10, printf ("yield file\n"));
    xs_atomic_add (fi->readcount, -1);
#endif
}



int xs_fileinfo_loaddata(xs_fileinfo* fi, const char *path) { //assumes valid xs_fileinfo* and fi->status==1
    int maxsize = 10000000, f;
    if (fi==0 || fi->stat_ret || fi->status==0 || fi->is_directory) return -1;

    //status is 2
    xs_atomic_spin (xs_atomic_swap (fi->status, 1, 2)!=1);
    if (fi->size>(size_t)maxsize) {
        if (fi->data) {if (_use_MMAP_) munmap (fi->data, fi->size); else free(fi->data);}
        fi->data = 0;
    } else if (_use_MMAP_==0) {
        fi->data = fi->data ? (char*)realloc(fi->data, fi->size) : (char*)malloc (fi->size);
    }

    //still got it?
    if ((_use_MMAP_ ? 1 : (fi->data!=0)) && (f=xs_open (path, O_RDONLY|O_BINARY, 0))!=0) {
        //read it
        if (fi->data==0)    fi->data = mmap (0, fi->size, PROT_READ, MAP_SHARED, f, 0);
        else                fi->size = read(f, fi->data, fi->size);
        close(f);
    } else if (fi->data) {
        //error
        free(fi->data);
        fi->data = 0;
    }

    fi->status = 1;
    return fi->data ? 0 : -3;
}

int xs_fileinfo_get(xs_fileinfo** fip, const char *path, int load_data) {
    int deltasecs = 2;
    khiter_t iter;
    xs_fileinfo *fptr, *fpd, **fhan, tfi;
    int result;
    time_t ct = time(0);
    if (fip==0) return -1;
    
    //does it exists?
    xs_fileinfo_global_lockstatus(0, 1);
    iter = kh_get_statptr (gxs_statptr, path);
    fhan = (iter==kh_end(gxs_statptr)) ? 0 : &kh_val(gxs_statptr, iter);
    *fip = fptr = fhan ? *fhan : 0;
   
    //create it
    if (fhan==0) {
        xs_fileinfo_global_lockstatus(1, 2);
        if ((fptr=(xs_fileinfo*)calloc(1,sizeof(xs_fileinfo)))!=0) {
            path = xs_strdup(path);
            iter = kh_put_statptr (gxs_statptr, path, &result);
            if (result==0) {assert(0);} //result should >0 always (e.g. it does not exist)
            fhan = &kh_val(gxs_statptr, iter);
            *fhan = fptr;
            *fip  = fptr;
        } else {
            xs_fileinfo_global_lockstatus(2, 0);
            memset(fip, 0, sizeof(*fip)); //error if we don't
            return -108;
        }
        xs_fileinfo_global_lockstatus(2, 1);
    }
    assert(fptr);

    //done
    xs_fileinfo_global_lockstatus(1, 0);

    //check time
    if (1 && ct-fptr->check_time>deltasecs && fptr->status==10) {
        //status is 3
        xs_atomic_spin (xs_atomic_swap (fptr->status, 10, 3)!=10);
        if (ct-fptr->check_time>deltasecs)    {
            //check the directory mod time rather than the file
            if (0 && /*fptr->dirptr==0 && */fptr->is_directory==0) { //directory time not updating with edits??? $$$SREE - investigate...
                char dirpath[PATH_MAX];
                xs_path_getdirectory(dirpath, sizeof(dirpath), path);
                if (xs_fileinfo_get(&fpd, dirpath, 0)==0) {
                    if (fptr->folder_mod == fpd->modification_time) 
                            fptr->status = 10; //restore
                    else fptr->folder_mod = fpd->modification_time;
                } 
            }
            if (fptr->status==3) fptr->status = 0;  //reset
        } else fptr->status = 10; //restore
    }

    //update it
    if (xs_atomic_swap (fptr->status, 0, 1)==0) {
        char* olddata = 0;
        //get it now - status is 1
        xs_logger_info ("refreshed %s", path);
        (void)xs_stat(path, &tfi); //<--- call "real" stat
        tfi.check_time = ct;
        tfi.status = fptr->status;
        tfi.folder_mod = fptr->folder_mod;
        if (load_data && tfi.modification_time!=fptr->modification_time) {
            if (fptr->data && _use_MMAP_) {munmap (fptr->data, fptr->size); fptr->data = 0;}
            (void)xs_fileinfo_loaddata(&tfi, path);
            olddata = fptr->data;
        } else tfi.data = fptr->data;
        xs_atomic_spin (fptr->readcount);
        *fptr = tfi; //copy
        fptr->status = 10; //status is 10
        if (olddata) free(olddata);
    } else while (fptr->status<10) sched_yield(); //wait for it

    return fptr->stat_ret;
}

#endif //_xs_FILEINFO_IMPL_
#endif //_xs_IMPLEMENTATION_
