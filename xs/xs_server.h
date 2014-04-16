// =================================================================================================================
// xs_server.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_SERVER_H_
#define _xs_SERVER_H_

typedef struct xs_server_ctx		xs_server_ctx;
typedef struct xs_async_connect		xs_async_connect;
typedef struct xs_conn				xs_conn;

// =================================================================================================================
// function declarations
// =================================================================================================================
xs_server_ctx*			xs_server_create	(const char* rel_path, const char* root_path);
int						xs_server_listen	(xs_server_ctx* ctx, xs_conn* conn);
int						xs_server_active	(xs_server_ctx* ctx);
xs_server_ctx*			xs_server_stop		(xs_server_ctx* ctx);
xs_server_ctx*			xs_server_destroy	(xs_server_ctx* ctx);


#endif //header







// =================================================================================================================
//  implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_SERVER_IMPL_
#define _xs_SERVER_IMPL_

#include "xs_arr.h"
#include "xs_queue.h"

typedef struct xs_server_ctx {
	char server_name[PATH_MAX];
	char document_root[PATH_MAX];
	struct xs_async_connect* xas;
	xs_queue writeq;
}xs_server_ctx;


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
	xs_arr	pathdata;
	xs_arr	dir;
} xs_dirlist;

xs_dirlist* xs_dirlist_create() {
	return (xs_dirlist*)calloc (sizeof(xs_dirlist), 1);
}

xs_dirlist* xs_dirlist_destroy(xs_dirlist* dl) {
	if (dl==0)		return 0;
	xs_arr_destroy	(dl->pathdata);
	xs_arr_destroy	(dl->dir);
	return 0;
};

int xs_dirlist_add(xs_dirlist* dl, char* filename, xs_fileinfo* fdp) {
	xs_fileinfo* dir;
	char* pathdata;
	int len = strlen(filename);
	if (len==0) return -50;

	pathdata	= xs_arr_add(char,			dl->pathdata, filename, len+1);
	dir			= xs_arr_add(xs_fileinfo,	dl->dir, fdp, 1);
	if (pathdata==0 || dir==0) return -108;
	pathdata[len]	= 0; //terminate
	dir->userinfo	= (int)(pathdata - xs_arr_ptr(char, dl->pathdata)); //name
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
	
	if ((dir=opendir(dirpath))==0)	return 0;
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

size_t xs_http_dirresponse(xs_server_ctx* ctx, xs_conn* conn, const char* origpath, const char* path) {
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
		if (*h=='d')	sortinfo[0] = 'd';
		else            sortinfo[0] = 'a';
		if (h[1])		sortinfo[1] = h[1];
	}
	switch (sortinfo[1]) {
		case 'n':	xs_ptr_qsort(fdp, xs_arr_count(dl->dir), sizeof(*fdp), (xs_ptr_compareproc)(sortinfo[0]=='a'?xs_dircompare_an:xs_dircompare_dn), name); break;
		case 'd':	xs_ptr_qsort(fdp, xs_arr_count(dl->dir), sizeof(*fdp), (xs_ptr_compareproc)(sortinfo[0]=='a'?xs_dircompare_ad:xs_dircompare_dd), name); break;
		case 's':	xs_ptr_qsort(fdp, xs_arr_count(dl->dir), sizeof(*fdp), (xs_ptr_compareproc)(sortinfo[0]=='a'?xs_dircompare_as:xs_dircompare_ds), name); break;
	}
	if (sortinfo[0]=='d')	sortinfo[2]='a';
	else					sortinfo[2]='d';

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
		if (appendSlash)	result+=(*proc) (conn, "<tr><td>^<a href='%s/..'><b>..</b></a></td><td>-</td><td>-</td></tr>\n", origpath);
		else				result+=(*proc) (conn, "<tr><td>^<a href='%s..'><b>..</b></a></td><td>-</td><td>-</td></tr>\n",  origpath);
	}

	//loop over directories
	for (i=0; i<xs_arr_count(dl->dir); i++) {
		if (appendSlash) xs_sprintf (tpath, sizeof(tpath), "%s/%s", origpath, name+fdp[i].userinfo, sizeof(tpath));
		else			 xs_sprintf (tpath, sizeof(tpath), "%s%s",  origpath, name+fdp[i].userinfo, sizeof(tpath));
		if (fdp[i].is_directory)		result+=(*proc) (conn, "<tr><td>/<a href='%s'><b>%s</b></a></td><td>%s</td><td>-</td></tr>\n",
										        			tpath, name+fdp[i].userinfo, 
										        			xs_timestr(date, sizeof(date), &fdp[i].modification_time)+4);
		else if (1)						result+=(*proc) (conn, "<tr><td>&nbsp;<a href='%s'>%s</a></td><td>%s</td><td>%zd</td></tr>\n",
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
//   Copyright (c) 2004-2013 Sergey Lyubka
//   see full notice in xs_SSL.h
static const struct {
    const char *extension;
    size_t ext_len;
    const char *mime_type;
} gxs_builtin_mime_types[] = {
    {".html", 5, "text/html"},
    {".htm", 4, "text/html"},
    {".shtm", 5, "text/html"},
    {".shtml", 6, "text/html"},
    {".css", 4, "text/css"},
    {".js",  3, "application/x-javascript"},
    {".ico", 4, "image/x-icon"},
    {".gif", 4, "image/gif"},
    {".jpg", 4, "image/jpeg"},
    {".jpeg", 5, "image/jpeg"},
    {".png", 4, "image/png"},
    {".svg", 4, "image/svg+xml"},
    {".txt", 4, "text/plain"},
    {".torrent", 8, "application/x-bittorrent"},
    {".wav", 4, "audio/x-wav"},
    {".mp3", 4, "audio/x-mp3"},
    {".mid", 4, "audio/mid"},
    {".m3u", 4, "audio/x-mpegurl"},
    {".ogg", 4, "audio/ogg"},
    {".ram", 4, "audio/x-pn-realaudio"},
    {".xml", 4, "text/xml"},
    {".json",  5, "text/json"},
    {".xslt", 5, "application/xml"},
    {".xsl", 4, "application/xml"},
    {".ra",  3, "audio/x-pn-realaudio"},
    {".doc", 4, "application/msword"},
    {".exe", 4, "application/octet-stream"},
    {".zip", 4, "application/x-zip-compressed"},
    {".xls", 4, "application/excel"},
    {".tgz", 4, "application/x-tar-gz"},
    {".tar", 4, "application/x-tar"},
    {".gz",  3, "application/x-gunzip"},
    {".arj", 4, "application/x-arj-compressed"},
    {".rar", 4, "application/x-arj-compressed"},
    {".rtf", 4, "application/rtf"},
    {".pdf", 4, "application/pdf"},
    {".swf", 4, "application/x-shockwave-flash"},
    {".mpg", 4, "video/mpeg"},
    {".webm", 5, "video/webm"},
    {".mpeg", 5, "video/mpeg"},
    {".mov", 4, "video/quicktime"},
    {".mp4", 4, "video/mp4"},
    {".m4v", 4, "video/x-m4v"},
    {".asf", 4, "video/x-ms-asf"},
    {".avi", 4, "video/x-msvideo"},
    {".bmp", 4, "image/bmp"},
    {".ttf", 4, "application/x-font-ttf"},
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
size_t xs_http_writefiledata(xs_conn* conn, const char* path, xs_fileinfo* fdp, size_t rs, size_t re, int blocking) {
    char buf[8192];
	const xs_httpreq* req = xs_conn_getreq(conn);
    int sock=-1, fi;
    size_t tot=0, outtot=re-rs, w;
    //FILE* f;

    if (xs_conn_writable(conn)==0 && blocking==0) return 0;
    if (1 && fdp->data) {
		//from cache
		do {
            w = 256<<10;
            if (tot+w>outtot) w=outtot-tot;
            if (xs_conn_writable(conn)==0) {if (blocking==0) return tot; xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);}
            w = xs_conn_write_ (conn, fdp->data+rs+tot, (size_t)w, (tot+w<outtot) ? MSG_MORE : 0);
            if (xs_conn_error(conn)==exs_Error_WriteBusy) {
                if (blocking==0) return tot; 
                xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);
                xs_conn_cachepurge(conn);
            } else if (xs_conn_error (conn) || w==0) {xs_printf ("err3 %zd %d\n", w, xs_conn_error (conn)); break;}//{if (result==0 && tot==0) tot=w; break;}
            tot += w;
        } while (tot<outtot);
	} else {
		//from file
        //xs_logger_info ("reading file %s", path);
        size_t bsize    = 256<<10; 
        char *bufn      = malloc(bsize = bsize>outtot ? outtot : bsize);
        if (bufn==0)    {bufn=buf; bsize=sizeof(buf);}

#if 0
        fi = xs_open(path, O_RDONLY|O_BINARY, 0);
		//f = fopen (path, "rb");
		if (fi) {
            char* fptr = (char*)mmap (0, outtot, PROT_READ, MAP_SHARED, fi, rs);
			#ifndef _WIN32
			fcntl(fi, F_SETFD, FD_CLOEXEC);
			#endif
			do {
				w = bsize;
                if (tot+w>outtot) w=outtot-tot;
                if (0 || xs_conn_writable(conn)==0) {
                    if (blocking==0) {munmap (fptr, outtot); close(fi); return tot;} 
                    xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);
                }
				//w = read (fi, bufn, w);
				w = xs_conn_write_ (conn, fptr+tot, w, (tot+w<outtot) ? MSG_MORE : 0);
                if (xs_conn_error (conn) || w==0)  break;
                tot += w;
			} while (tot<outtot);
            munmap (fptrre, outtot);
			close (fi);
		}  
#elif 1
        fi = xs_open(path, O_RDONLY|O_BINARY, 0);
		if (fi) {
			#ifndef _WIN32
			fcntl(fi, F_SETFD, FD_CLOEXEC);
			#endif
			lseek (fi, (size_t)rs, SEEK_SET);
			do {
				w = bsize;
                if (tot+w>outtot) w=outtot-tot;
                if (xs_conn_writable(conn)==0) {
                    if (blocking==0) {close(fi); return tot;} 
                    xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);
                }
                w = read (fi, bufn, w); 
				w = xs_conn_write_ (conn, bufn, w, (tot+w<outtot) ? MSG_MORE : 0);
                tot += w>0 ? w : 0;
                if (xs_conn_error(conn)==exs_Error_WriteBusy) {
                    if (blocking==0) {printf ("err1\n"); close(fi); return tot;} 
                    xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);
                    xs_conn_cachepurge (conn);
                } else if (xs_conn_error(conn) || w==0) break;
			} while (tot<outtot);
			close (fi);
		}  
#else
		f = xs_fopen (path, "rb");
		if (f) {
			#ifndef _WIN32
			fcntl(fileno(f), F_SETFD, FD_CLOEXEC);
			if (rs) fseek (f, rs, SEEK_SET);
			#else
			fseek (f, (size_t)rs, SEEK_SET);
			#endif
			do {
				w = bsize;
                if (tot+w>outtot) w=outtot-tot;
                if (0 || xs_conn_writable(conn)==0) {
                    if (blocking==0) {fclose(f); return tot;} 
                    xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);
                }
				w = fread (bufn, 1, w, f);
                //xs_printf ("writing %zd but managed ", w);
				w = xs_conn_write_ (conn, bufn, w, (tot+w<outtot) ? MSG_MORE : 0);
                //xs_printf ("%zd\n", w);
                if (xs_sizet_negzero(w)) break;//{if (result==0 && tot==0) tot=w; break;}
                tot += w;
			} while (tot<outtot);
			fclose (f);
		}  
#endif

        if (bufn!=buf) free(bufn);
    }

	if (sock>=0) xs_sock_setnonblocking(sock, 0);
	
	//didn't write data properly
	if (tot!=re-rs && blocking) {
		//shit.
		xs_logger_error ("shit. %d!=%d", (int)tot, (int)(re-rs));
        xs_http_setint ((xs_httpreq* )req, exs_Req_KeepAlive, 0);
	    xs_http_setint (xs_conn_getreq(conn), exs_Req_Status, 0);
	}

    return tot;
}

char* xs_http_etag (char* str, int ssize, const xs_fileinfo *fd) {
	xsuint64 mt;
	mt = (xsuint64)fd->modification_time;
	xs_sprintf (str, ssize, "\"%llx.%zx\"", mt, fd->size);
	return str;
}
static char xs_http_notmodified(const xs_httpreq *req, const xs_fileinfo *fd) {
	char etag[64];
	const char *h = xs_http_getheader(req, "If-None-Match");
	if (h && !xs_strcmp(h, xs_http_etag(etag, sizeof(etag), fd)))	return 1;
	h = xs_http_getheader(req, "If-Modified-Since");
	return 0;
}

size_t xs_http_fileresponse(xs_server_ctx* ctx, xs_conn* conn, const char* path, int dobody) {
	const xs_httpreq* req = xs_conn_getreq(conn);
	xs_fileinfo fd, *fdp=&fd;
	const char* h, *ver;
	xsuint64 rs, re;
	size_t result=0;
	char etag[64], range[128]="";
	int statuscode=200, sock=0, dohdr=1;
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
		return xs_http_dirresponse (ctx, conn, xs_http_get(req, exs_Req_URI), path);
	} else if (0 || xs_http_notmodified(req, fdp)) {
		return xs_conn_write_httperror (conn, 304, "Not Modified", ""); // <---- shortcut out
		statuscode = 304;
		xs_strlcpy (statusmsg, "Not Modified", sizeof(statusmsg));
		dobody = 0;
	}

    //lock it
    xs_fileinfo_lock(fdp);
	rs=0;
	re=fdp->size;

    //range header check
	h = xs_http_getheader (req, "Range");
	if (h==0 || sscanf(h, "bytes=%" INT64_FMT "-%" INT64_FMT, &rs, &re)!=2) {
		rs = 0;
		re = fdp->size;
	} else if (rs>re || re>fdp->size) { //unsigned, so no need to check if less than 0
		result += xs_conn_write_httperror (conn, 400, "Bad Range Request", "Requested bytes %zd-%zd of %zd", rs, re, fdp->size);
		dohdr = dobody = 0;
	} else if (statuscode==200) { //only change code for 200 and range request
		statuscode = 206;
		xs_strlcpy (statusmsg, "Partial Content", sizeof(statusmsg));
		xs_sprintf (range, sizeof(range), "Content-Range: bytes %zd-%zd/%zd\r\n", rs, re, fdp->size);
	}

	//write header
    if (dohdr) {
	    ver = xs_http_get(req, exs_Req_Version);
	    if (ver && !strcmp(ver, "1.0")) ver=0;
	    //if (xs_conn_writable(conn)==0) xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);
	    //xs_timestr(modt, sizeof(modt), &fdp->modification_time);
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
		    xs_http_getint(req, exs_Req_KeepAlive) ? "keep-alive" : "close",
		    range);
	    xs_conn_header_done(conn, dobody && (re-rs)!=0);
    }


	//write body
    xs_http_setint (xs_conn_getreq(conn), exs_Req_Status, statuscode);
	if (dobody==1 && (re-rs)!=0) { //note dobody==1 --- allows HEAD requests to specify dobody=2
#if 0
	    FILE* f;
        char buf[8192];
        int amt;

        size_t tot=0, outtot=(size_t)(re-rs), w;
		if (fdp->data) {
			//from cache
			do {
                w = 256<<10;
                if (tot+w>outtot) w=outtot-tot;
			    if (xs_conn_writable(conn)==0) xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);
                w = xs_conn_write (conn, fdp->data+rs+tot, (size_t)w);
                if (w<0) break;
                tot += w;
            } while (tot<outtot);
            result += tot;
		} else {
			//from file
            xs_logger_info ("reading file %s", path);
			f = fopen (path, "rb");
			if (f) {
			    #ifndef _WIN32
				fcntl(fileno(f), F_SETFD, FD_CLOEXEC);
				if (rs) fseek (f, rs, SEEK_SET);
			    #else
				fseek (f, (size_t)rs, SEEK_SET);
			    #endif
				do {
					w = sizeof(buf);
                    if (tot+w>outtot) w=outtot-tot;
					w = fread (buf, 1, w, f);
					if (xs_conn_writable(conn)==0) xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);
					w = xs_conn_write_ (conn, buf, w, (tot+w<outtot) ? MSG_MORE : 0);
                    if (w<0) break;
                    tot += w;
				} while (tot<outtot);
				fclose (f);
                result += tot;
			}  
		}

		if (sock) xs_sock_setnonblocking(sock, 0);
	
		//didn't write data properly
		if (result!=re-rs) {
			//shit.
			xs_logger_error ("shit. %d!=%d", (int)result, (int)(re-rs));
            xs_http_setint ((xs_httpreq* )req, exs_Req_KeepAlive, 0);
	        xs_http_setint (xs_conn_getreq(conn), exs_Req_Status, 0);
		}
#elif 0
        writeq_data wqdata;
        wqdata.seq = xs_conn_seqinc(conn);
        wqdata.conn = conn;
        wqdata.rs = rs;
        wqdata.re = re;
        wqdata.path = xs_strdup (path);
        xs_conn_inc(conn);
        xs_queue_push (&ctx->writeq, &wqdata, 1);
#else
        result += xs_http_writefiledata (conn, path, fdp, (size_t)rs, (size_t)re, 1);
#endif
	}

    //unlock
    xs_fileinfo_unlock(fdp);

	//success
	return result;
}


typedef struct writeq_data {
	xs_atomic seq;
	char* path;
	xsuint64 rs, re;
	xs_conn *conn;
} writeq_data;

size_t xs_http_writefiledata(xs_conn* conn, const char* path, xs_fileinfo* fdp, size_t rs, size_t re, int blocking);

void writeq_proc (xs_queue* qs, writeq_data *wqd, void* privateData) {
 	xs_fileinfo *fdp;
    size_t result;
    xs_fileinfo_get (&fdp, wqd->path, 1);
    xs_fileinfo_lock (fdp);
    //xs_printf ("writing      %lld -- %lld :: %lld\n", (wqd->re-wqd->rs), wqd->rs, wqd->re);
    result = xs_conn_writable(wqd->conn) ? xs_http_writefiledata (wqd->conn, wqd->path, fdp, (size_t)wqd->rs, (size_t)wqd->re, 0) : 0;
    xs_fileinfo_unlock (fdp);
    if (/*xs_sizet_neg(result)==0 && */result != (size_t)(wqd->re-wqd->rs) && xs_conn_error(wqd->conn)==0) {
        printf ("retrying %ld of %ld -- %ld :: %ld\n", (long)result, (long)(wqd->re-wqd->rs), (long)wqd->rs, (long)wqd->re);
        if (result>0) wqd->rs += result;
        xs_queue_push (qs, wqd, 1);
    } else {
        if (result==0) printf ("closed  %ld of %ld -- %ld :: %ld\n", (long)result, (long)(wqd->re-wqd->rs), (long)wqd->rs, (long)wqd->re);
        if (wqd->path) free(wqd->path);
        xs_conn_dec(wqd->conn);
    }
    (void)qs;
    (void)wqd;
    (void)privateData;
}





// ==================================================
//  core server code
// ==================================================
static void websocket_ready_handler(xs_conn *conn) {
    static const char *message = "server ready";
    xs_conn_write_websocket(conn, exs_WS_TEXT, message, strlen(message), 0);
}

// Arguments:
//   flags: first byte of websocket frame, see websocket RFC,
//          http://tools.ietf.org/html/rfc6455, section 5.2
//   data, datalen: payload data. Mask, if any, is already applied.
static int websocket_data_handler(xs_conn *conn, char *data, size_t datalen) {
    xs_conn_write_websocket(conn, exs_WS_TEXT, data, datalen, 0);
    //xs_conn_write_websocket (conn, exs_WS_CONNECTION_CLOSE, 0, 0, 0);

   // Returning zero means stoping websocket conversation.
   // Close the conversation if client has sent us "exit" string.
   return memcmp(data, "exit", 4);
}

xs_atomic gcc=0, cbcc=0;
int xs_server_cb (struct xs_async_connect* xas, int message, xs_conn* conn) {
	xs_server_ctx* ctx = (xs_server_ctx*)xs_async_getuserdata (xas);
	char buf[1024];
	int n, s, err=0, sock=0, rr, lcount=0;
	xs_atomic rcount = xs_conn_rcount(conn);
	const char* h;
    //if (message!=exs_Conn_Idle) xs_logger_info ("entering %d", message);
	switch (message) {
		case exs_Conn_New:
			break;
		case exs_Conn_Read:
			cbcc++;
			while (err==0 && ((n=xs_conn_httpread(conn, buf, sizeof(buf)-1, &rr))>0 || rr)) {
				lcount++;
				if ((s=xs_conn_state (conn))==exs_Conn_Complete || n>0) {// || s==exs_Conn_Websocket) {//n>0) {
					const xs_httpreq* req =xs_conn_getreq(conn);
					xs_atomic_inc (gcc);
					h = xs_http_get(req, exs_Req_Method);
					if (!strcmp(h, "GET") || !strcmp(h, "HEAD")) {
                        if ((s=xs_http_getint(req, exs_Req_Upgrade))) {
                            if (s==1) {
                                if (1) {//xs_websocket_response (ctx, conn)==0) {
                                    websocket_ready_handler(conn);
                                    if (n>0 && websocket_data_handler(conn, buf, n)==0)
                                        xs_conn_write_websocket (conn, exs_WS_CONNECTION_CLOSE, 0, 0, 0);
                              } 
                            } else {
                                    if (n>0 && websocket_data_handler(conn, buf, n)==0)
                                         xs_conn_write_websocket (conn, exs_WS_CONNECTION_CLOSE, 0, 0, 0);
                            }
                        } else if (1) {
                            size_t result;
							char path[PATH_MAX]="";
							n = xs_strappend (path, sizeof(path), ctx->document_root);
							xs_strlcat (path+n, xs_http_get (req, exs_Req_URI), sizeof(path));

							//xs_conn_write_header (conn, msghdr, sizeof(msghdr)-1);
							//xs_conn_write (conn, msg, sizeof(msg)-1);
							result = xs_http_fileresponse (ctx, conn, path,*h=='G'); //GET vs HEAD
			                if (1)
                                xs_logger_debug ("%s - - \"%s %s HTTP/%s\" %d %zd", 
                                            //xs_sock_addrtostr (ipaddr, sizeof(ipaddr), xs_conn_getsockaddr(conn)),//"ADDR", 
                                            xs_conn_getsockaddrstr (conn),//"ADDR", 
                                            xs_http_get (req, exs_Req_Method), 
                                            xs_http_get (req, exs_Req_URI),
                                            xs_http_get (req, exs_Req_Version),
                                            xs_http_getint (req, exs_Req_Status), result);
						} else if (0) {
							if (xs_conn_writable(conn)==0) xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);
							xs_conn_write_header (conn, msghdr, sizeof(msghdr)-1);
							if (xs_conn_writable(conn)==0) xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);
							xs_conn_write (conn, msg, sizeof(msg)-1);
							if (sock) xs_sock_setnonblocking(sock, 1);
						} else {
							xs_conn_write_httperror (conn, 200, "OK", //"Seriously. File not found.\n");
											"100xxxxxxx"
											"xxxxxxxxxx"
											"xxxxxxxxxx"
											"xxxxxxxxxx"
											"xxxxxxxxxx"
											"xxxxxxxxxx"
											"xxxxxxxxxx"
											"xxxxxxxxxx"
											"xxxxxxxxxx"
											"xxxxxxxxx0");
						}
						if (xs_http_getint(req, exs_Req_KeepAlive)==0) {
							err = exs_Conn_Close;
						}
					} else xs_logger_error ("HTTP method '%s' not handled %s", xs_http_get (req, exs_Req_Method), xs_conn_getsockaddrstr (conn));
				} else { 
					if (s==exs_Conn_Error) err = xs_conn_error(conn);
					//else printf ("%ld something other state %d\n", gcc, s);
                    switch (err) {
                        case exs_Error_HeaderTooLarge:  xs_conn_write_httperror (conn, 414, "Header Too Large", "Max Header Sizer Exceeded");   break;
                        case exs_Error_InvalidRequest:  xs_conn_write_httperror (conn, 501, "Not Supported", "Invalid Request");                break;
                    }
					if (n<=0) break;
				}
			}
            if (n<0) err=xs_conn_error(conn);
			if (n!=0 && err==0) break;
            err=err;

			//fallthrough...
			if (err && err!=exs_Conn_Close) xs_logger_error ("closing connections from %s %d", xs_conn_getsockaddrstr (conn), err);

		case exs_Conn_Error:
		case exs_Conn_Close:
			xs_conn_dec(conn);
			err = exs_Conn_Close;
			break;
	}

	if (gexit) {
		xs_logger_fatal ("quitting....");
		xs_async_destroy(xas);
	}

	//if (message==exs_Conn_Read && rcount==xs_conn_rcount(conn))
	//	xs_logger_error ("read error %d : %d", message, rcount);

    //if (message!=exs_Conn_Idle) xs_logger_info ("exiting %d - %d", message, lcount);
	return err;
}

// ==================================================
//  core server mgmt
// ==================================================
xs_server_ctx* xs_server_create(const char* rel_path, const char* root_path) {
    xs_server_ctx* ctx = calloc(sizeof(xs_server_ctx), 1);
	struct xs_async_connect* xas=xs_async_create(32);
	xs_strlcpy(ctx->server_name, xs_server_name(), sizeof(ctx->server_name));
	xs_path_setabsolute (ctx->document_root, sizeof(ctx->document_root), rel_path, root_path);
	xs_queue_create (&ctx->writeq, sizeof(writeq_data), 1024*10, (xs_queue_proc)writeq_proc, NULL);
	xs_queue_launchthreads (&ctx->writeq, 2, 0);
	xs_async_setuserdata(xas, ctx);
	ctx->xas = xas;
	return ctx;
}

xs_async_connect* xs_server_xas (xs_server_ctx* ctx) {
    return ctx ? ctx->xas : 0;
}

int	xs_server_listen (xs_server_ctx* ctx, xs_conn* conn) {
	if (ctx==0) return -50;
    ctx->xas = xs_async_listen (ctx->xas, conn, xs_server_cb);
	return (ctx->xas)!=0;
}

int xs_server_active(xs_server_ctx* ctx) {
	return ctx ? xs_async_active(ctx->xas) : 0;
}

xs_server_ctx* xs_server_stop (xs_server_ctx* ctx) {
	if (ctx==0) return 0;
	ctx->xas = xs_async_destroy (ctx->xas);
	return ctx;
}


xs_server_ctx* xs_server_destroy (xs_server_ctx* ctx) {
	if (ctx==0) return 0;
	xs_server_stop(ctx);
	free(ctx);
	return 0;
}


#endif //_xs_SERVER_IMPL_
#endif //_xs_IMPLEMENTATION_