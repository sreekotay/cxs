#ifdef WIN32
    #define FD_SETSIZE      1024
    #include <ws2tcpip.h>   //if you want IPv6 on Windows, this has to be first
    #define sleep(a)        Sleep((a)*1000)
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "xs/xs_types.h"
#include "xs/xs_logger.h"
#include "xs/xs_server.h"
#include "xs/xs_connection.h"
#include "xs/xs_fileinfo.h"
#include "xs/xs_json.h"
#include "xs/xs_arr.h"
#include "xs/xs_printf.h"
#include "xs/xs_posix_emu.h"

int server_handler (struct xs_async_connect* xas, int message, xs_conn* conn);
int do_benchmark (int argc, char *argv[]);
void load_redirfile();

void doit();

// =================================================================================================================
// main
// =================================================================================================================
int main(int argc, char *argv[]) {
    int err=0, accesslog=0, i, port = 8080, singlethreadaccept = 0;
    xs_server_ctx *ctx;
    xs_async_connect* xas;
    char sslkey[] = "default_webxs_ssl_key.pem";

    //init all
    xs_server_init_all (1);

    if (argc<2 || strcmp(argv[1],"-b")) {
        //parse command line
	    for (i=1; i<argc; i++) {
		    if (strcmp(argv[i],"-a")==0) {
                xs_logger_info ("access log on"); 
                accesslog = 1;
            } else if (strcmp(argv[i],"-p")==0) {
                if (i<argc) port = atoi(argv[(++i)]);
                xs_logger_info ("port %d", port);
            } else if (strcmp(argv[i],"-v")==0) {
                printf ("%s\n", xs_server_name());
                exit(1);
            } else if (strcmp(argv[i],"-s")==0) {
                xs_logger_info("accept on unique thread");
                singlethreadaccept = 1;
            } else if (strcmp(argv[i],"-h")==0) {
                printf ("Usage: %s [options]\n    -p port#\n    -a \n\n", xs_server_name());
                exit(1);
            } else if (strcmp(argv[i], "-r")==0) {
                load_redirfile();
            }
	    }
    
        //start server and connections
        ctx = xs_server_create(".", argv[0], server_handler);
        //xs_server_auth_file(ctx, ".", "passwd"); // <--- replace with your htpasswd file
        xas = xs_server_xas(ctx); 
        if (accesslog==0) xs_logger_level(exs_Log_Error, exs_Log_Info);

        //main server loop
	    xs_server_listen     (ctx, port, 0);
	    xs_server_listen_ssl (ctx, 443,  0, sslkey, sslkey, sslkey);
        if (singlethreadaccept) xs_async_lock(xas);
        //doit();

	    while (xs_server_active(ctx)) {sleep(1);} //your app's event loop

	    xs_logger_fatal ("---- done ----");
    } else do_benchmark (argc, argv);

    //all done
    xs_server_terminate_all();
    xs_logger_counter_print();
    return 7734;
}


// =================================================================================================================
// server code example
// =================================================================================================================
xs_atomic gcount=0, gocount=0, gredir=0;
xs_arr gurlarr={0};
int server_handler (struct xs_async_connect* xas, int message, xs_conn* conn) {
    char buf[1024];
    const char wsmsg[] = "server ready";

    int n, rr=1, err=0, s;
    xs_httpreq* req;
    switch (message) {
       case exs_Conn_Read:
            while (err==0 && rr) {
                n = xs_conn_httpread(conn, buf, sizeof(buf)-1, &rr);
                s = xs_conn_state (conn);
                err = xs_conn_error (conn);
                req = xs_conn_getreq (conn);

                if (err) 
                    err = exs_Conn_Close;
                else if (n>=0 && err==0) {
                    switch (s) {
                        case exs_Conn_Header:   
                            //printf ("conn new   %d\n", (int)xs_atomic_inc(gcount));   
                            break;

                        case exs_Conn_Complete: 
                            //printf ("conn done  %d\n", (int)xs_atomic_inc(gocount));  
                            if (xs_arr_count(gurlarr)) {
                                int w = xs_atomic_inc(gredir)%xs_arr_count(gurlarr);
                                char str[1024];
                                xs_json_tag* jt = xs_arr_ptr(xs_json_tag, gurlarr) + w;
                                xs_sprintf (str, sizeof(str), "Moved\r\nLocation: %.*s", jt->len, jt->ptr);
                                xs_conn_write_httperror (conn, 302, str, "");
                                xs_conn_httplogaccess(conn,0);
                                err = exs_Conn_Close;
                            } else if (0) {
                                xs_conn_write_httperror (conn, 200, "OK", "xxxxxxxxxx");
                            } else {
                                err = xs_server_handlerequest ((xs_server_ctx*)xs_async_getuserdata (xas), conn);
                                if (xs_http_getint(req, exs_Req_KeepAlive)==0)
                                    err = exs_Conn_Close;
                            }
                            break;

                        case exs_Conn_WSNew:
                            xs_conn_write_websocket(conn, exs_WS_TEXT, wsmsg, strlen(wsmsg), 0);
                            break;

                        case exs_Conn_WSFrameEnd:
                        case exs_Conn_WSFrameRead:
                            if (n) xs_conn_write_websocket(conn, exs_WS_TEXT, buf, n, 0);
                            if (n>=4 && memcmp (buf,"exit",4)==0)
                                xs_conn_write_websocket (conn, exs_WS_CONNECTION_CLOSE, 0, 0, 0);
                            break;
                    }
                }
            }
        break;
    }
    return err;
}

void load_redirfile() {
    xs_fileinfo* fi;
    xs_json* js;
    xs_json_tag jt = {0};
    int err=0;
    xs_fileinfo_get (&fi, "redirect.json", 1);
    xs_fileinfo_lock (fi);
    if ((js = xs_json_create ((const char*)fi->data, fi->size))) {
      while ((err=xs_json_next (js, &jt, 1))==0) { 
          switch (jt.type) {
               case exs_JSON_ObjectStart: xs_logger_info ("{");   break;
               case exs_JSON_ObjectEnd:   xs_logger_info ("}");   break;
               case exs_JSON_ArrayStart:  xs_logger_info ("[");   break;
               case exs_JSON_ArrayEnd:    xs_logger_info ("]");   break;

               case exs_JSON_Key:
                  xs_logger_info ("\"%.*s\" : ", jt.len, jt.ptr);
                  break;

               case exs_JSON_Value:
                  xs_logger_info ("\"%.*s\"", jt.len, jt.ptr);
                  xs_arr_add(xs_json_tag, gurlarr, &jt, 1);
                  break;
          }
      }
      xs_json_destroy(js);
    }
    xs_fileinfo_unlock (fi);
    xs_fileinfo_unloaddata (fi);
    if (err<0) xs_logger_error("json redirect error: %d", err);
}


// =================================================================================================================
// client code example
// =================================================================================================================
xs_atomic gtotaldl=1000, gconcurrent=10, gpipeline=0, gbytes=0, gconncount=0;
int gexit=0;
typedef struct bench {
    char *host, *method, *path;
    int port;
    xs_atomic count, total, bytes, refcount, concurrent, concurrenttotal;
} bench;
typedef struct bench_tl {
    bench*      bn;
    xs_atomic   lcount, ltotal;

} bench_tl;

int do_div (int t, int d, int n) {return (t/d) + (n==0 ? t-(t/d)*d: 0);}
xs_async_connect* launch_connects(bench* bn, int concurrent, int total);

int do_benchmark (int argc, char *argv[]) {
	int i, tc=1, port=80;
    xs_uri* u = 0;
    xs_arr xa = {0};
    bench* bn = 0;

    //parse command line
	for (i=1; i<argc; i++) {
		if (strcmp(argv[i],"-p")==0)
			{if (i+1<argc) port=(int)atof(argv[++i]);}
		else if (strcmp(argv[i],"-b")==0)
			{}
		else if (strcmp(argv[i],"-pipeline")==0)
			{gpipeline=1;}
		else if (strcmp(argv[i],"-t")==0)
			{if (i+1<argc) tc=(int)atoi(argv[++i]);}
		else if (strcmp(argv[i],"-n")==0)
			{if (i+1<argc) gtotaldl=(int)atoi(argv[++i]);}
		else if (strcmp(argv[i],"-c")==0)
			{if (i+1<argc) gconcurrent=(int)atoi(argv[++i]);}
		else if ((u=xs_uri_destroy(u))==0) {
			u = xs_uri_create(argv[i], ((argv[i])[0]!='/')*2);
			xs_logger_info ("target URL: host %s, path %s, port %d\n", u->host?u->host:"none", u->path?u->path:"none", u->port);
		}
	}

    //launch connects
    if (1) {
        char buf[1024], *ptr;
        char method[] = "GET";  
        char* host = u&&u->host ? u->host : "127.0.0.1";
        char *path = u&&u->path ? u->path : "/100.html";
        port = u&&u->port ? u->port : port;

        if (1) {
            xs_conn* conn;
            int err=0, n, rr=1, s;
            err = xs_conn_open (&conn, host, port, 0);
            if (err==0) n   = xs_conn_httprequest (conn, host, method, path);
            if (err==0) err = xs_conn_error(conn);
            if (err==0) n   = xs_conn_header_done (conn, 0);
            if (err==0) err = xs_conn_error(conn);

            while (err==0 && xs_conn_state(conn)!=exs_Conn_Complete) {
                n = xs_conn_httpread (conn, buf, sizeof(buf), &rr);
                s = xs_conn_state(conn);
                err = xs_conn_error(conn);
                if (s==exs_Conn_Header && n>=0 &&
                    (n=xs_conn_headerinspect(conn, &ptr))>0) {
                    printf ("=========== header start\n");
                    printf ("%.*s", n, ptr);
                    printf ("=========== header end\n");
                } else if (n>0) {}//printf ("%.*s", n, buf); //print body
            }
            xs_conn_destroy (conn);

            if (err) {
                xs_logger_error ("unable to access URL: %d", err);
                return err;
            }
        }

        bn = (bench*)calloc(sizeof(bench), 1);
        if (gconcurrent<tc) {gconcurrent = tc; xs_logger_info ("adjusting concurrency to: %d", tc);}
        bn->concurrenttotal = gconcurrent;
        bn->host = u&&u->host ? u->host : "127.0.0.1";
        bn->port = u&&u->port ? u->port : port;
        bn->method = "GET";
        bn->path = u&&u->path ? u->path : "/100.html";
        bn->total = gtotaldl;
        if (1) {
            xs_atomic_add(bn->refcount, tc);
	        for (i=0; i<tc; i++)
		        xs_arr_push(xs_async_connect*, xa, launch_connects(bn, do_div(bn->concurrenttotal, tc, i), do_div(gtotaldl, tc, i)));
        } else gexit=1;
    }


    while (gexit==0) {sleep(1);}


    //done
    if (u) xs_uri_destroy(u);
    for (i=0; i<xs_arr_count(xa); i++) {
        xs_async_destroy (xs_arr(xs_async_connect*, xa, i));
    }
    if (bn) free(bn);
    return 0;
}


double mytime() {
	struct timespec ts;
    int err;
    double t;
    err = clock_gettime (CLOCK_REALTIME, &ts);
    t = ts.tv_sec + ts.tv_nsec * 1e-9;
    return t;
}



double gtime=0;
xs_atomic gretry=0;
int perform_get(xs_async_connect* xas, bench_tl* btl, xs_conn* conn) {
    int err=0, n;
    bench* bn = btl->bn;
    xs_atomic bncount = -1;
    bncount = xs_atomic_inc(bn->count) + 1;
    if ((bn->total<10 || (bncount%(bn->total/10))==0) && bncount) 
        xs_logger_info ("progress... [0x%p]: %ld", btl, bncount);
    while (xs_atomic_inc(btl->lcount)<btl->ltotal) {
        n   = xs_conn_httprequest (conn, bn->host, bn->method, bn->path);
        if (err==0) err = xs_conn_error(conn);
        n   = xs_conn_header_done (conn, 0);
        if (err==0) err = xs_conn_error(conn);
        if (err==exs_Error_WriteBusy) break;
        if (gpipeline==0) break;
    }  
    if (err && err!=exs_Error_WriteBusy) 
        xs_logger_error ("error %d -- conn error%d", err, xs_conn_error(conn));
    if (bncount>=bn->total) {
        double t = ((double)(mytime() - gtime)); 
        if (bncount>=bn->total) {
            xs_logger_info ("count %ld time %ld", (long)bncount, (long)(t*1000));
            xs_logger_info ("requests per second: %ld   bytes: %ld", (long)(bncount/t+.5), (long)gbytes);
            gexit = 1;
        }
        err = exs_Conn_Close;
        xs_async_stop (xas); 
    } else if (bncount<0) {
        err = exs_Conn_Close;
    }

    return err;
}
int benchmark_cb (struct xs_async_connect* xas, int message, xs_conn* conn) {
    char buf[1024];
    const char wsmsg[] = "server ready";
    int n, rr=1, err=0, s;
    bench_tl* btl = (bench_tl*)xs_async_getuserdata (xas);
    bench* bn = btl ? btl->bn : 0;

    if (bn==0) 
        err = exs_Conn_Close;
    else
    switch (message) {
        case exs_Conn_Write:
        case exs_Conn_New:
            if (bn->count==0) gtime = mytime();
            xs_atomic_dec(bn->count);
            err = perform_get(xas, btl, conn); 
            if (err && err!=exs_Error_WriteBusy) {} 
            else xs_atomic_inc(bn->concurrent);
            break;

        case exs_Conn_Read:
            while (err==0 && rr) {
                n = xs_conn_httpread(conn, buf, sizeof(buf)-1, &rr);
                err = xs_conn_error (conn);
                s = xs_conn_state(conn);
                if (n>0) xs_atomic_add(gbytes, n);

                if (err) 
                    err = exs_Conn_Close;
                else if (s==exs_Conn_Complete) {
                    err = perform_get(xas, btl, conn); 
                    if (err) {if (err!=exs_Error_WriteBusy) {}  else err = exs_Conn_Write;}
                }
            }
            break;


        case exs_XAS_Destroy: 
            if (xs_atomic_dec(bn->refcount)<=1) {
                xs_async_setuserdata(xas, 0);
                xs_logger_info ("---- bench complete - bytes %ld", (long)gbytes);
            }
            break;

        case exs_Conn_Close:
        case exs_Conn_Error:
            err = xs_conn_error(conn);
            if ((bn&&xs_atomic_dec(bn->concurrent)<=bn->concurrenttotal) &&
                (bn->count<bn->total) &&
                (err==exs_Conn_Close || err==10054)) {
                //we were shut down gracefully, so try again
                xs_conn* conn2;
                err = xs_conn_open (&conn2, bn->host, bn->port, 0);
                if (err==0) xas = xs_async_read (xas, conn2, 0);
                if (err)
                    xs_logger_error ("Connection error %s: %d", bn->host, err);
            } else if (err && bn->count<bn->total) 
                xs_logger_error ("Connection error %s: %d", bn->host, err);
            else
                err = err;
            break;
    }

    return err;
}

xs_async_connect* launch_connects(bench* bn, int concurrent, int total) {
    int err=0, i;
    xs_async_connect* xas=xs_async_create (8, benchmark_cb);
    xs_conn* conn=0;
    bench_tl* btl = calloc(sizeof(bench_tl), 1);
    btl->bn = bn;
    btl->ltotal = total;//(bn->total + concurrent - 1) / concurrent;
    printf ("=============== total per concurrent %d\n", (int) btl->ltotal);
    xs_async_setuserdata (xas, btl);
    for (i=0; i<concurrent && gexit==0; i++) {
        err = xs_conn_open (&conn, bn->host, bn->port, 0);
        if (err) break;
        xas = xs_async_read (xas, conn, benchmark_cb);
    }
    if (err) {xs_logger_error ("Benchmark error %d", err); gexit=1;}
    return xas;
}



// =================================================================================================================
// required libraries
// =================================================================================================================
#define _xs_IMPLEMENTATION_
#include "xs/xs_logger.h"
#include "xs/xs_server.h"
#include "xs/xs_connection.h"
#include "xs/xs_json.h"
#include "xs/xs_pipe.h"
#include "xs/xs_process.h"
#undef _xs_IMPLEMENTATION_



//simple process test
/*
void doit () {
    xs_pipe pin, pout;
    pid_t pid;
    char buf[81920];
    int n, err;
    struct pollfd fd[2]={0};
    xs_pipe_open (&pin);
    xs_pipe_open (&pout);
    pid = xs_process_launch (0, "prout", 0, pin.r, pout.w);

    xs_process_closeonexec (pin.r);
    xs_process_closeonexec (pin.w);
    xs_process_closeonexec (pout.r);
    xs_process_closeonexec (pout.w);

    xs_sock_setnonblocking (pin.r, 1);
    xs_sock_setnonblocking (pin.w, 1);
    xs_sock_setnonblocking (pout.r, 1);
    xs_sock_setnonblocking (pout.w, 1);
#ifndef WIN32
    (void) closesocket(pin.r);
    (void) closesocket(pout.w);
    pin.r = pout.w = -1;
#endif

#if defined WIN32 && !defined xs_SOCKET_PIPE
    if (1) {
        DWORD mode=PIPE_READMODE_BYTE|PIPE_NOWAIT;
        SetNamedPipeHandleState ((HANDLE)_get_osfhandle (pout.r), &mode, 0, 0);
    }
#endif

    fd[0].events = POLLIN | POLLERR;
    fd[0].fd = pout.r;
    do {
#ifdef WIN32
        DWORD exitcode = 0;
        if (GetExitCodeProcess(pid, &exitcode)==0 || exitcode!=259)   break;
#endif
        if (poll (fd, 1, 10000)) {
            if (fd[0].revents & (POLLERR|POLLHUP|POLLNVAL)) {printf ("break\n"); break;}
            if (fd[0].revents & POLLIN)
            do {
                printf ("\n................................reading: %d.......................................\n", n);

                n = xs_pipe_read (&pout, buf, sizeof(buf)-1);
                err = xs_sock_err();
                if (n<0 && xs_sock_wouldblock(err))//||err==232)  
                    {n=1; break;}// printf ("waiting...\n"); continue;}
                if (n>0) {printf ("%.*s", n, buf);}
                else     {printf ("\ndone n: %d - err: %d\n", n, xs_sock_err()); }
            

            } while (n>0);

        } else n=1;
    } while (n>0);
    printf ("that's all she wrote!!.......................................n:[%d]\n", n);
    xs_pipe_close (&pin);
    xs_pipe_close (&pout);
    return;
}
*/
