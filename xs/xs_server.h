// =================================================================================================================
// xs_server.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_SERVER_H_
#define _xs_SERVER_H_

#include "xs_connection.h"

typedef struct xs_server_ctx        xs_server_ctx;

// =================================================================================================================
// function declarations
// =================================================================================================================
xs_server_ctx*          xs_server_create            (void);
void                    xs_server_auth_file         (xs_server_ctx* ctx, const char* rel_path, const char* root_path);
int                     xs_server_listen            (xs_server_ctx* ctx, int port, xs_async_callback* cb);
int                     xs_server_listen_ssl        (xs_server_ctx* ctx, int port, xs_async_callback* cb, const char* privateKeyPem, const char* certPem, const char* certChainPem);
int                     xs_server_active            (xs_server_ctx* ctx);       //returns >0 if active
xs_async_connect*       xs_server_xas               (xs_server_ctx* ctx); 
xs_server_ctx*          xs_server_stop              (xs_server_ctx* ctx);
xs_server_ctx*          xs_server_destroy           (xs_server_ctx* ctx);

int                     xs_server_init_all          (int startupstuff);         //setup signal handers, etc
int                     xs_server_stop_all          (int signal, void *dummy);  //callback for signalling -- (0, 0) is fine -- this is internal
int                     xs_server_terminate_all     ();

int                     xs_server_sethandler        (xs_server_ctx* ctx, const char* server_path, xs_async_callback* p, const char* rel_path, const char* root_path);
int                     xs_server_handlerequest     (xs_server_ctx* ctx, xs_conn* conn);
int                     xs_server_auth_request      (xs_server_ctx* ctx, xs_conn* conn);

int                     xs_simpleresponse           (xs_conn* conn);            //for performance testing 

#endif //header






// =================================================================================================================
//  implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_SERVER_IMPL_
#define _xs_SERVER_IMPL_

#undef _xs_IMPLEMENTATION_
#include <assert.h>
#include "xs_startup.h"
#include "xs_arr.h"
#include "xs_queue.h"
#include "xs_fileinfo.h"
#include "xs_ssl.h"
#define _xs_IMPLEMENTATION_

struct xs_server_ctx {
    char server_name[PATH_MAX];
    char document_root[PATH_MAX];
    char* auth_file;
    struct xs_async_connect* xas;
    //xs_queue writeq;
};

xs_atomic gserverlistlock=0;
xs_array  gserverlist={0};

// ==================================================
//  directory stuff
// ==================================================
char* xs_dirnext(struct dirent **dep, char* path, int pathsize, DIR *dir, const char* dirpath) {
    struct dirent *de=readdir(dir);
    if (de==0) return 0;
    
    if (!strcmp(de->d_name, ".") ||
        !strcmp(de->d_name, ".."))
        path[0]=0;
    else xs_sprintf(path, pathsize, "%s%c%s", dirpath, '/', de->d_name);
    *dep=de;
    return path;
}


typedef struct xs_dirlist {
    char rootpath[PATH_MAX];
    xs_arr  pathdata;
    xs_arr  dir;
} xs_dirlist;

xs_dirlist* xs_dirlist_create() {
    return (xs_dirlist*)calloc (sizeof(xs_dirlist), 1);
}

xs_dirlist* xs_dirlist_destroy(xs_dirlist* dl) {
    if (dl==0)      return 0;
    xs_arr_destroy  (dl->pathdata);
    xs_arr_destroy  (dl->dir);
    return 0;
};

int xs_dirlist_add(xs_dirlist* dl, char* filename, xs_fileinfo* fdp) {
    xs_fileinfo* dir;
    char* pathdata;
    int len = strlen(filename);
    if (len==0) return -50;

    pathdata    = xs_arr_add(char,          dl->pathdata, filename, len+1);
    dir         = xs_arr_add(xs_fileinfo,   dl->dir, fdp, 1);
    if (pathdata==0 || dir==0) return -108;
    pathdata[len]   = 0; //terminate
    dir->userinfo   = (int)(pathdata - xs_arr_ptr(char, dl->pathdata)); //name
    return 0;
}

xs_dirlist* xs_dirscan(xs_conn* conn, const char *dirpath) {
    char path[PATH_MAX];
    DIR *dir;
    xs_fileinfo fd, *fdp=&fd;
    struct dirent *de;
    xs_dirlist* dl = xs_dirlist_create();
    (void)conn;
    if (dl==0) return 0;
    
    if ((dir=opendir(dirpath))==0)  return 0;
    xs_strlcpy (dl->rootpath, dirpath, sizeof(dl->rootpath));
    while (xs_dirnext(&de, path, sizeof(path), dir, dirpath)!=0) {  
        if (path[0]==0) continue;
#ifdef _old_file_stuff_
        fd=*my_statHashGet(path);
#else
        xs_fileinfo_get(&fdp, path, 0);
#endif
        if (path[0] && fdp->stat_ret==0) {//old way with stat(): xs_stat(path, &fd)==0) {
            if (xs_dirlist_add(dl, de->d_name, fdp)!=0) {
                //error
            }
        }
    }
    closedir(dir);
    return dl;
}

int xs_dircompare_an(const xs_fileinfo *a, const xs_fileinfo *b, const char* comp) {int c=b->is_directory-a->is_directory; return c?c:xs_strcmp_case(comp+a->userinfo,comp+b->userinfo);}
int xs_dircompare_dn(const xs_fileinfo *a, const xs_fileinfo *b, const char* comp) {int c=b->is_directory-a->is_directory; return c?c:xs_strcmp_case(comp+b->userinfo,comp+a->userinfo);}
                                                                                                                           
int xs_dircompare_ad(const xs_fileinfo *a, const xs_fileinfo *b, const char* comp) {int c=b->is_directory-a->is_directory; (void)comp; c=c?c:a->modification_time>b->modification_time?-1:1; return c;}
int xs_dircompare_dd(const xs_fileinfo *a, const xs_fileinfo *b, const char* comp) {int c=b->is_directory-a->is_directory; (void)comp; c=c?c:b->modification_time>a->modification_time?-1:1; return c;}
                                                                                                                           
int xs_dircompare_as(const xs_fileinfo *a, const xs_fileinfo *b, const char* comp) {int c=b->is_directory-a->is_directory; c=c?c:a->size<b->size?-1:(a->size>b->size?1:0); return c?c:xs_dircompare_an(a,b,comp);}
int xs_dircompare_ds(const xs_fileinfo *a, const xs_fileinfo *b, const char* comp) {int c=b->is_directory-a->is_directory; c=c?c:b->size<a->size?-1:(b->size>a->size?1:0); return c?c:xs_dircompare_dn(a,b,comp);}

size_t xs_http_dirresponse(xs_conn* conn, const char* origpath, const char* path) {
    typedef int (printproc)(xs_conn*, const char *fmt, ...);
    printproc *proc;
    size_t result=0;
    char sortinfo[]="an "; //sory = ascending-name
    int i, appendSlash;
    xs_fileinfo* fdp;
    char tpath[PATH_MAX];
    char* name, date[64];
    const char* ver, *h;
    xs_dirlist* dl = xs_dirscan (conn, path);
    xs_httpreq* req = xs_conn_getreq(conn);
    if (req==0 || dl==0)
        return xs_conn_write_httperror (conn, 400, "Bad Directory Request", path);

    //write header
    ver = xs_http_get(req, exs_Req_Version);
    if (ver && !strcmp(ver, "1.0")) ver=0;
    proc = ver ? xs_conn_printf_chunked : xs_conn_printf;
    xs_conn_printf_header(conn, 
            "HTTP/%s 200 OK\r\n"
            "Server: %s\r\n"
            "Date: %s\r\n"
            "Connection: %s\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "%s", 
            ver ? ver : "1.0",
            xs_server_name(), xs_timestr_now(),
            ver ? "keep-alive" : "close", 
            ver ? "Transfer-Encoding: chunked\r\n" : "");
    xs_conn_header_done(conn, 1);

    //sort
    fdp = xs_arr_ptr(xs_fileinfo, dl->dir);
    name = xs_arr_ptr(char, dl->pathdata);
    h = xs_http_get(req, exs_Req_Query);
    if (h && *h) {
        if (*h=='d')    sortinfo[0] = 'd';
        else            sortinfo[0] = 'a';
        if (h[1])       sortinfo[1] = h[1];
    }
    switch (sortinfo[1]) {
        case 'n':   xs_ptr_qsort(fdp, xs_arr_count(dl->dir), sizeof(*fdp), (xs_ptr_compareproc)(sortinfo[0]=='a'?xs_dircompare_an:xs_dircompare_dn), name); break;
        case 'd':   xs_ptr_qsort(fdp, xs_arr_count(dl->dir), sizeof(*fdp), (xs_ptr_compareproc)(sortinfo[0]=='a'?xs_dircompare_ad:xs_dircompare_dd), name); break;
        case 's':   xs_ptr_qsort(fdp, xs_arr_count(dl->dir), sizeof(*fdp), (xs_ptr_compareproc)(sortinfo[0]=='a'?xs_dircompare_as:xs_dircompare_ds), name); break;
    }
    if (sortinfo[0]=='d')   sortinfo[2]='a';
    else                    sortinfo[2]='d';

    //write body
    result+=(*proc)(conn, "<html>\n<head>\n<title>'%s' Directory Listing</title>\n", origpath);
    result+=(*proc)(conn, 
                      "<style>\n"
                        "body{background:#fff;font:16px Georgia,Palatino,Serif;color:#aaa;padding:20px 0 0 20px;}\n"
                        "table{border-spacing:0px 0}td,th{text-align:left;}th{padding:10px 0px;}td{padding:2px 10px 0px 10px;}\n"
                        "h1{font-size:24px;font-weight:200;color:#ccc}\n"
                        "tr:nth-child(even){background: #f0f0f0;}\n"
                        "tr:hover{background:rgba(0,0,0,0.15)}\n"
                        "thead tr:hover{background:0}\n"
                        "a {text-decoration:none;}\n"
                      "</style>\n");
    result+=(*proc)(conn, "</head>\n<body>\n");
    result+=(*proc)(conn, "<h1>Directory Listing for '%s'</h1>\n<pre><table>\n", origpath);
    result+=(*proc)(conn,
              "<thead>\n<tr>"
              "<th><a href=\"?%cn\">Name</a></th>\n"
              "<th><a href=\"?%cd\">Mod-Date</a></th>\n"
              "<th><a href=\"?%cs\">Size</a></th></tr>\n</thead>\n",
              sortinfo[1]=='n'?sortinfo[2]:sortinfo[0],
              sortinfo[1]=='d'?sortinfo[2]:sortinfo[0],
              sortinfo[1]=='s'?sortinfo[2]:sortinfo[0]
                );

    //add parent directory
    appendSlash = (origpath[strlen(origpath)-1]!='/');
    if (xs_strcmp(origpath, "/")) {
        if (appendSlash)    result+=(*proc) (conn, "<tr><td>^<a href='%s/..'><b>..</b></a></td><td>-</td><td>-</td></tr>\n", origpath);
        else                result+=(*proc) (conn, "<tr><td>^<a href='%s..'><b>..</b></a></td><td>-</td><td>-</td></tr>\n",  origpath);
    }

    //loop over directories
    for (i=0; i<xs_arr_count(dl->dir); i++) {
        if (appendSlash) xs_sprintf (tpath, sizeof(tpath), "%s/%s", origpath, name+fdp[i].userinfo, sizeof(tpath));
        else             xs_sprintf (tpath, sizeof(tpath), "%s%s",  origpath, name+fdp[i].userinfo, sizeof(tpath));
        if (fdp[i].is_directory)        result+=(*proc) (conn, "<tr><td>/<a href='%s'><b>%s</b></a></td><td>%s</td><td>-</td></tr>\n",
                                                            tpath, name+fdp[i].userinfo, 
                                                            xs_timestr(date, sizeof(date), &fdp[i].modification_time)+4);
        else if (1)                     result+=(*proc) (conn, "<tr><td>&nbsp;<a href='%s'>%s</a></td><td>%s</td><td>%zd</td></tr>\n",
                                                            tpath, name+fdp[i].userinfo, 
                                                            xs_timestr(date, sizeof(date), &fdp[i].modification_time)+4,
                                                            fdp[i].size);
    }
    result+=(*proc)(conn, "</table></pre>\n</body>\n</html>\n");
    result+=(*proc)(conn, 0); //terminate chunked transfer (noop otherwise, anyway)
    xs_dirlist_destroy(dl);

    //success
    xs_http_setint (xs_conn_getreq(conn), exs_Req_Status, 200);
    return result;
}




// ==================================================
//  mime stuff
// ==================================================
// nifty mime struct from mongoose.c
//   NOTE: derived from the MIT licensed version of mongoose (pre 4.0)
//   Copyright (c) 2004-2013 Sergey Lyubka
//   see full notice in xs_SSL.h
static const struct {
    const char *extension;
    size_t ext_len;
    const char *mime_type;
} gxs_builtin_mime_types[] = {
    {".html",       5, "text/html"},
    {".htm",        4, "text/html"},
    {".shtm",       5, "text/html"},
    {".shtml",      6, "text/html"},
    {".css",        4, "text/css"},
    {".js",         3, "application/x-javascript"},
    {".ico",        4, "image/x-icon"},
    {".gif",        4, "image/gif"},
    {".jpg",        4, "image/jpeg"},
    {".jpeg",       5, "image/jpeg"},
    {".png",        4, "image/png"},
    {".svg",        4, "image/svg+xml"},
    {".txt",        4, "text/plain"},
    {".torrent",    8, "application/x-bittorrent"},
    {".wav",        4, "audio/x-wav"},
    {".mp3",        4, "audio/x-mp3"},
    {".mid",        4, "audio/mid"},
    {".m3u",        4, "audio/x-mpegurl"},
    {".ogg",        4, "audio/ogg"},
    {".ram",        4, "audio/x-pn-realaudio"},
    {".xml",        4, "application/xml"},
    {".json",       5, "text/json"},
    {".xslt",       5, "application/xml"},
    {".xsl",        4, "application/xml"},
    {".ra",         3, "audio/x-pn-realaudio"},
    {".doc",        4, "application/msword"},
    {".exe",        4, "application/octet-stream"},
    {".zip",        4, "application/x-zip-compressed"},
    {".xls",        4, "application/excel"},
    {".tgz",        4, "application/x-tar-gz"},
    {".tar",        4, "application/x-tar"},
    {".gz",         3, "application/x-gunzip"},
    {".arj",        4, "application/x-arj-compressed"},
    {".rar",        4, "application/x-arj-compressed"},
    {".rtf",        4, "application/rtf"},
    {".pdf",        4, "application/pdf"},
    {".swf",        4, "application/x-shockwave-flash"},
    {".mpg",        4, "video/mpeg"},
    {".webm",       5, "video/webm"},
    {".mpeg",       5, "video/mpeg"},
    {".mov",        4, "video/quicktime"},
    {".mp4",        4, "video/mp4"},
    {".m4v",        4, "video/x-m4v"},
    {".asf",        4, "video/x-ms-asf"},
    {".avi",        4, "video/x-msvideo"},
    {".bmp",        4, "image/bmp"},
    {".ttf",        4, "application/x-font-ttf"},
    {0, 0, 0}
};

const char *xs_find_mime_type(const char *path) {
    const char *ext;
    size_t i, path_len;
    
    path_len = strlen(path);
    for (i = 0; gxs_builtin_mime_types[i].extension != 0; i++) {
        ext = path + (path_len - gxs_builtin_mime_types[i].ext_len);
        if (path_len > gxs_builtin_mime_types[i].ext_len && ext[0]=='.' &&
            xs_strcmp_case(ext, gxs_builtin_mime_types[i].extension) == 0)
            return gxs_builtin_mime_types[i].mime_type;
    }
    
    return "text/plain";
}


// ==================================================
//  core file handling stuff
// ==================================================
#ifdef WIN32
#include <io.h>
#else
#include <sys/mman.h>
#ifndef O_BINARY
#define O_BINARY 0 
#endif
#endif

char* xs_http_etag (char* str, int ssize, const xs_fileinfo *fd) {
    xsuint64 mt;
    mt = (xsuint64)fd->modification_time;
    xs_sprintf (str, ssize, "\"%llx.%zx\"", mt, fd->size);
    return str;
}
static char xs_http_notmodified(const xs_httpreq *req, const xs_fileinfo *fd) {
    char etag[64];
    const char *h = xs_http_getheader(req, "If-None-Match");
    if (h && !xs_strcmp(h, xs_http_etag(etag, sizeof(etag), fd)))   return 1;
    h = xs_http_getheader(req, "If-Modified-Since");
    return 0;
}

#ifdef WIN32
#define INT64_FMT		"I64d"
#else
#define INT64_FMT		"lld"
#endif


size_t xs_http_fileresponse(xs_async_connect* xas, xs_conn* conn, const char* path, int dobody) {
    const xs_httpreq* req = xs_conn_getreq(conn);
    xs_fileinfo fd, *fdp=&fd;
    const char* h, *ver;
    xsint64 rs, re;
    size_t result=0;
    char etag[64], range[128]="";
    int statuscode=200, dohdr=1, haverangehdr=0;
    char statusmsg[128]="OK";

    //xs_stat(path, &fd);
#ifdef _old_file_stuff_
    fd = *my_statHashGet(path);//instead of xs_stat(path, &fd);
#else
    xs_fileinfo_get (&fdp, path, 1);
#endif

    //early out
    if (fdp->stat_ret!=0) {
        return xs_conn_write_httperror (conn, 404, "File not found", "Seriously. File not found.\n");
    } else if (fdp->is_directory) {
        return xs_http_dirresponse (conn, xs_http_get(req, exs_Req_URI), path);
    } else if (0 || xs_http_notmodified(req, fdp)) {
        return xs_conn_write_httperror (conn, 304, "Not Modified", ""); // <---- shortcut out
    }

    //lock it
    xs_fileinfo_lock(fdp);
    rs=0;
    re=fdp->size;

    //range header check
    if ((h=xs_http_getheader (req, "Range"))) {
        if (     sscanf(h, "bytes=%" INT64_FMT "-%" INT64_FMT, &rs, &re)==2)   haverangehdr = 1;
        else if (sscanf(h, "bytes=%" INT64_FMT "-", &rs)==1)                   haverangehdr = 1;
        else if (sscanf(h, "bytes=-%" INT64_FMT, &re)==1)                      haverangehdr = 1;
    }
    if (h==0 || haverangehdr==0) { //reset
        rs = 0;
        re = fdp->size;
        if (h) {
            char* ptr;
            int n=xs_conn_headerinspect(conn, &ptr);
            xs_logger_warn ("failed to parse range header %s for %s", h, path);
            printf ("=========== header start\n");
            printf ("%.*s", n, ptr);
            printf ("=========== header end\n");
        }
    } else if (rs>re || re>fdp->size) { //unsigned, so no need to check if less than 0
        xs_logger_warn ("bad range header %s for %s", h, path);
        result += xs_conn_write_httperror (conn, 400, "Bad Range Request", "Requested bytes %zd-%zd of %zd", rs, re, fdp->size);
        dohdr = dobody = 0;
    } else if (statuscode==200) { //only change code for 200 and range request
        statuscode = 206;
        xs_logger_info ("valid range header %s for %s", h, path);
        xs_strlcpy (statusmsg, "Partial Content", sizeof(statusmsg));
        xs_sprintf (range, sizeof(range), "Content-Range: bytes %zd-%zd/%zd\r\n", rs, re, fdp->size);
    }

    //write header
    if (dohdr) {
        ver = xs_http_get(req, exs_Req_Version);
        if (ver && !strcmp(ver, "1.0")) ver=0;
        xs_conn_printf_header(conn,
            "HTTP/%s %d %s\r\n"
            "Content-Length: %zd\r\n"
            "Server: %s\r\n"
            "Date: %s\r\n"
            //"Last-Modified: %s\r\n" //redundant/not suggested - per google
            "Etag: %s\r\n"
            "Content-Type: %s\r\n"
            "Accept-Ranges: bytes\r\n"
            "Connection: %s\r\n"
            "%s",
            ver ? ver : "1.0", statuscode, statusmsg, (size_t) (dobody ? (re-rs) : 0), xs_server_name(), 
            xs_timestr_now(), /*modt,*/ xs_http_etag(etag, sizeof(etag), fdp), 
            xs_find_mime_type(path),//strstr(path, "htm") ? "text/html" : "text/plain", 
            (xs_http_getint(req, exs_Req_KeepAlive) && dobody!=2) ? "keep-alive" : "close",
            range);
        xs_conn_header_done(conn, dobody && (re-rs)!=0);
    }


    //write body
    if (dobody==1 && (re-rs)!=0) { //note dobody==1 --- allows HEAD requests to specify dobody=2
        if (0) {
            xs_fileinfo_unlock(fdp);
            xs_async_write_filedata(xas, conn, path, (size_t)rs, (size_t)re);
            xs_fileinfo_lock (fdp);
        } else result += xs_conn_write_filedata (conn, path, fdp, (size_t)rs, (size_t)re, 1);
    }

    //unlock
    xs_fileinfo_unlock(fdp);

    //success
    return result;
}

int xs_server_handlerequest(xs_server_ctx* ctx, xs_conn* conn) {
    int err=0;
    xs_httpreq* req  = xs_conn_getreq(conn);
    const char* h    = xs_http_getint (req, exs_Req_Upgrade) ? 0 : xs_http_get(req, exs_Req_Method);

    //====================
    // request
    //====================
    if (h && (!strcmp(h, "GET") || !strcmp(h, "HEAD"))) {

        if(ctx->auth_file && xs_server_auth_request(ctx, conn) != 0) {
            xs_conn_write_httperror (conn, 401, "Not Authorized\r\nWWW-Authenticate: Basic", "");
            return exs_Conn_Close;
        }

        if (0)                         
            xs_conn_httplogaccess (conn, xs_conn_write_httperror (conn, 200, "OK", "simple"));
        else if (1) {
            char path[PATH_MAX]="";
            int n = xs_strappend (path, sizeof(path), ctx->document_root);
            xs_strlcat (path+n, xs_http_get (req, exs_Req_URI), sizeof(path));

            if (0) xs_conn_write_httperror (conn, 200, "OK", "test");
            else xs_conn_httplogaccess
                    (conn, xs_http_fileresponse (ctx->xas, conn, path, h? *h=='G' : 1)); //GET vs HEAD
        }
    } else if (h) xs_logger_error ("HTTP method '%s' not handled %s", h ? h : "UNSPECIFIED", xs_conn_getsockaddrstr (conn));
    if (xs_http_getint(req, exs_Req_KeepAlive)==0) {
        err = exs_Conn_Close;
    }

    return err;
}

int xs_server_auth_request(xs_server_ctx* ctx, xs_conn* conn) {
    int ret=-1;
    const xs_httpreq* req;
    const char *a, *pass, *cur, *encrypted;
    char* authfile;
    char plaintext[256], salt[3];
	
    req = xs_conn_getreq (conn);
    a = xs_http_getheader (req, "Authorization");

    if (!a) return ret;

    if (strncmp (a, "Basic", 5) == 0) {
	    // advance to beginning of encoded data
        a += 6 * sizeof(char);	

        // decode
        xs_b64_decode (plaintext, sizeof(plaintext), (const xsuint8*)a, strlen(a));

        // find password after colon
        pass = strchr (plaintext, ':');
        if (pass == 0) return ret;
        pass++;

        authfile = xs_strdup(ctx->auth_file); // cause strtok is destructive

        // locate user in htpasswd file
        cur = strtok (authfile, "\n\r");
        while (cur != NULL) {
            if (strncmp (cur, plaintext, pass - plaintext - 1) == 0) {
                // found user
                // advance cur to point to salt
                cur = strchr (cur, ':') + 1;
                strncpy (salt, cur, 2);
                salt[2]=0;

                // encrypt the plaintext password using the salt from the password file
                encrypted = xs_DES_crypt (pass, salt);
                if (strcmp (encrypted, cur) == 0) {
                    // great success!
                    ret = 0;
                    break;
                }
            }
	        cur = strtok (NULL, "\n\r");
        }

        free(authfile);
    }
    else if (strncmp(a, "Digest", 6) == 0) {
        return ret;	// we'll support Digest here later...
    }
    
    return ret;
}

int xs_simpleresponse(xs_conn* conn) {
    int err=0, rr=0, n;
    char buf[1024];
    char msghdr[] = "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 10\r\n"
                    "Server: webxs/0.5\r\n"
                    "Date: Wed, 11 Jun 2014 02:20:26 GMT\r\n"
                    "Etag: \"52ae0a21.64\"\r\n"
                    "Content-Type: text/html\r\n"
                    "Accept-Ranges: bytes\r\n"
                    "Connection: keep-alive\r\n"
                    ;
    while (err==0 && rr) {
        n = xs_conn_read(conn, buf, sizeof(buf)-1, &rr);
        err = xs_conn_error (conn);
        if (err==0) xs_atomic_inc (gscount);
    }
    if (err==0) {
        xs_atomic_inc (gswcount);
        xs_conn_write_header (conn, msghdr, sizeof(msghdr)-1);
        xs_conn_header_done (conn, 1);
        xs_conn_write (conn, "xxxxxxxxxx", 10);
    } else {
        if (err != exs_Conn_Close)
            printf ("error ---------------- %d: %d-%d\n", err, (int)gscount, (int)gswcount);
    }
    return err;
}


// ==================================================
//  core server mgmt
// ==================================================
xs_server_ctx* xs_server_create(void) {
    xs_server_ctx* ctx = (xs_server_ctx*)calloc(sizeof(xs_server_ctx), 1);
    struct xs_async_connect* xas=xs_async_create(8, 0);
    ctx->xas = xas;

    xs_atomic_spin (xs_atomic_swap(gserverlistlock,0,1)!=0);
    xs_arr_add (xs_server_ctx*, gserverlist, &ctx, 1); 
    xs_atomic_spin (xs_atomic_swap(gserverlistlock,1,0)!=0);
    return ctx;
}

int xs_server_sethandler (xs_server_ctx* ctx, const char* server_path, xs_async_callback* p, const char* rel_path, const char* root_path) {
    if (ctx==0) return -50;
    if (server_path==0) {
        xs_strlcpy(ctx->server_name, xs_server_name(), sizeof(ctx->server_name));
        xs_path_setabsolute  (ctx->document_root, sizeof(ctx->document_root), rel_path, root_path);
        xs_async_setuserdata (ctx->xas, ctx);
        xs_async_setcallback (ctx->xas, p);
    } else return -2;
    return 0;
}

void xs_server_auth_file(xs_server_ctx* ctx, const char* rel_path, const char* root_path) {
    xs_fileinfo* fi;
    char fd[PATH_MAX];

    xs_path_setabsolute (fd, PATH_MAX, root_path, rel_path);
    if(xs_fileinfo_get (&fi, fd, 1)) return;
    if(!fi->size) return;

    xs_fileinfo_lock (fi);

    ctx->auth_file = (char*) malloc(fi->size+1);
    memcpy(ctx->auth_file, fi->data, fi->size);
    ctx->auth_file[fi->size] = 0;

    xs_fileinfo_unlock (fi);
    xs_fileinfo_unloaddata (fi);
}

xs_async_connect* xs_server_xas (xs_server_ctx* ctx) {
    return ctx ? ctx->xas : 0;
}

int xs_server_listen (xs_server_ctx* ctx, int port, xs_async_callback* cb) {
    int err;
    xs_conn *conn4=0, *conn6=0;
    if (ctx==0) return -50;
    if (cb==0 && ctx->xas) cb = xs_async_getcallback (ctx->xas);
#ifdef USE_IPV6
 	err = xs_conn_listen(&conn6, port, 0, 1);   //v6 socket (done 
    if (err) xs_logger_error ("listen v6 [%d]: %d se:%d", port, err, xs_sock_err());
    ctx->xas = xs_async_listen (ctx->xas, conn6, cb);
#endif
#if (!defined USE_IPV6) || defined WIN32
    err = xs_conn_listen(&conn4, port, 0, 0);   //v4 socket (redundant on linux)
	if (err) xs_logger_error ("listen v4 [%d]: %d se:%d", port, err, xs_sock_err());
    ctx->xas = xs_async_listen (ctx->xas, conn4, cb);
#endif
    return conn4 || conn6;
}

int xs_server_listen_ssl (xs_server_ctx* ctx, int port, xs_async_callback* cb, const char* privateKeyPem, const char* certPem, const char* certChainPem) {
    int err;
    xs_conn *conn4=0, *conn6=0;
    if (ctx==0) return -50;
    if (cb==0 && ctx->xas) cb = xs_async_getcallback (ctx->xas);
#ifdef USE_IPV6
 	err = xs_conn_listen(&conn6, port, 1, 1);   //v6 socket (done 
    if (err) xs_logger_error ("listen v6 [%d]: %d se:%d", port, err, xs_sock_err());
    if (conn6) err = xs_SSL_set_certs((SSL_CTX*)xs_conn_sslctx(conn6), privateKeyPem, certPem, certChainPem);

	if (err) xs_logger_error ("SSL init error [%d]: %d se:%d", port, err, xs_sock_err());
    else     ctx->xas = xs_async_listen (ctx->xas, conn6, cb);
#endif
#if (!defined USE_IPV6) || defined WIN32
    err = xs_conn_listen(&conn4, port, 1, 0);   //v4 socket (redundant on linux)
	if (err) xs_logger_error ("listen v4 [%d]: %d se:%d", port, err, xs_sock_err());
    if (conn4) err = xs_SSL_set_certs((SSL_CTX*)xs_conn_sslctx(conn4), privateKeyPem, certPem, certChainPem);

	if (err) xs_logger_error ("SSL init error [%d]: %d se:%d", port, err, xs_sock_err());
    else     ctx->xas = xs_async_listen (ctx->xas, conn4, cb);
#endif
    return conn4 || conn6;
}

int xs_server_active(xs_server_ctx* ctx) {
    return ctx ? xs_async_active(ctx->xas) : 0;
}

xs_server_ctx* xs_server_stop (xs_server_ctx* ctx) {
    if (ctx==0) return 0;
    xs_async_stop (ctx->xas);
    return ctx;
}


xs_server_ctx* xs_server_destroy (xs_server_ctx* ctx) {
    int i;
    if (ctx==0) return 0;
    xs_server_stop(ctx);
    ctx->xas = xs_async_destroy (ctx->xas);
    if(ctx->auth_file) free(ctx->auth_file);
    xs_atomic_spin (xs_atomic_swap(gserverlistlock,0,1)!=0);
    for (i=0; i<xs_arr_count (gserverlist) && xs_arr(xs_server_ctx*, gserverlist, i)!=ctx; i++)  {}
    if (i<xs_arr_count (gserverlist)) {
        xs_arr_remove (xs_server_ctx*, gserverlist, i, 1); 
    } else {assert(0);}
    xs_atomic_spin (xs_atomic_swap(gserverlistlock,1,0)!=0);
    free(ctx);
    return 0;
}

int xs_server_init_all(int startupstuff) {
    xs_logger_init();
    if (startupstuff) xs_startup(exs_Start_All, xs_server_stop_all, 0);
    xs_fileinfo_init();
	xs_SSL_initialize ();	
    return 0;
}

int xs_server_stop_all(int signal, void* dummy) {
    int i;
    signal; dummy;
    xs_atomic_spin (xs_atomic_swap(gserverlistlock,0,1)!=0);
    for (i=xs_arr_count (gserverlist)-1; i>=0; i--) {
        xs_server_ctx* ctx = xs_arr(xs_server_ctx*, gserverlist, i);
        xs_atomic_spin (xs_atomic_swap(gserverlistlock,1,0)!=0);
        xs_server_stop (ctx);
        xs_atomic_spin (xs_atomic_swap(gserverlistlock,0,1)!=0);
    }
    xs_atomic_spin (xs_atomic_swap(gserverlistlock,1,0)!=0);
    return 0;
}


int xs_server_terminate_all() {
    int i;
    xs_atomic_spin (xs_atomic_swap(gserverlistlock,0,1)!=0);
    for (i=xs_arr_count (gserverlist)-1; i>=0; i--) {
        xs_server_ctx* ctx = xs_arr(xs_server_ctx*, gserverlist, i);
        xs_atomic_spin (xs_atomic_swap(gserverlistlock,1,0)!=0);
        xs_server_destroy (ctx);
        xs_atomic_spin (xs_atomic_swap(gserverlistlock,0,1)!=0);
    }
    xs_atomic_spin (xs_atomic_swap(gserverlistlock,1,0)!=0);
    return 0;
}


//required implementations
#include "xs_socket.h"
#include "xs_connection.h"
#include "xs_ssl.h"
#include "xs_queue.h"
#include "xs_crc.h"
#include "xs_sha1.h"
//#include "xs_compress.h"
#include "xs_logger.h"
#include "xs_fileinfo.h"
#include "xs_server.h"
#include "xs_startup.h"

#endif //_xs_SERVER_IMPL_
#endif //_xs_IMPLEMENTATION_    
