#ifdef WIN32
    #define FD_SETSIZE      1024
    #include <ws2tcpip.h>   //if you want IPv6 on Windows, this has to be first
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "xs/xs_types.h"
#include "xs/xs_logger.h"
#include "xs/xs_server.h"
#include "xs/xs_connection.h"
#include "xs/xs_fileinfo.h"
#include "xs/xs_json.h"

xs_atomic gcount=0;

static void websocket_ready_handler(xs_conn *conn) {
    static const char *message = "server ready";
    xs_conn_write_websocket(conn, exs_WS_TEXT, message, strlen(message), 0);
}

static int websocket_data_handler(xs_conn *conn, char *data, size_t datalen) {
    xs_conn_write_websocket(conn, exs_WS_TEXT, data, datalen, 0);
    return memcmp(data, "exit", 4);
}

int myhandler (struct xs_async_connect* xas, int message, xs_conn* conn) {
    char buf[1024];
    int n, rr=1, err=0;
    switch (message) {
        case exs_Conn_WSRead:
            //websocket example
            while (err==0 && rr) {
                n = xs_conn_httpread(conn, buf, sizeof(buf)-1, &rr);
                err = xs_conn_error (conn);
                if (n>0 && websocket_data_handler (conn, buf, n)==0)
                    xs_conn_write_websocket (conn, exs_WS_CONNECTION_CLOSE, 0, 0, 0);
            }
            break;

        //http request
        /*
        case exs_Conn_HTTPNew:          printf ("conn new  %d\n", (int)xs_atomic_inc(gcount));  break;
        case exs_Conn_HTTPComplete:     printf ("conn done %d\n", (int)xs_atomic_inc(gcount));  break;
        */
    }
    return xs_async_defaulthandler (xas, message, conn);
}

int main(int argc, char *argv[]) {
    int err=0, accesslog=0, i, port = 8080, singlethreadaccept = 0;
    xs_server_ctx *ctx;
    xs_async_connect* xas;
    char sslkey[] = "default_webxs_ssl_key.pem";
    xs_fileinfo fi;
    xs_json* js;
    xs_json_tag jt = {0};

    //init all
    xs_server_init_all (1);

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
        }
	}

    if (argc>1 && strcmp(argv[1],"-a")==0) {
        printf ("access log on\n");
        accesslog = 1;
    } 

    /*
    xs_stat ("sample.json", &fi);
    fi.status = 1;
    xs_fileinfo_loaddata (&fi, "sample.json");
    js = xs_json_create ((const char*)fi.data, fi.size);
    while (xs_json_next (js, &jt)==0) {
        switch (jt.type) {
             case exs_JSON_ObjectStart: printf ("{\n");   break;
             case exs_JSON_ObjectEnd:   printf ("}\n");   break;
             case exs_JSON_ArrayStart:  printf ("[\n");   break;
             case exs_JSON_ArrayEnd:    printf ("]\n");   break;

            case exs_JSON_Key:
                printf ("\"%.*s\" : ", jt.len, jt.ptr);
                break;

            case exs_JSON_Value:
                printf ("\"%.*s\"\n", jt.len, jt.ptr);
                break;
        }
    }
    xs_fileinfo_loaddata (&fi, 0);
    */


    //start server and connections
    ctx = xs_server_create(".", argv[0]);
    xas = xs_server_xas(ctx); 
    if (accesslog==0) xs_logger_level(exs_Log_Error, exs_Log_Info);

    //main server loop
	xs_server_listen     (ctx, port, myhandler);
	xs_server_listen_ssl (ctx, 443,  myhandler, sslkey, sslkey, sslkey);
    if (singlethreadaccept) xs_async_lock(xas);
	while (xs_server_active(ctx)) {
        switch (getc(stdin)) {
            case 's': xs_async_print(xas); break;
        }
    }
	xs_logger_fatal ("---- done ----");

    //all done
    xs_server_terminate_all();
    xs_logger_counter_print();
    return 7734;
}





//required libraries
#define _xs_IMPLEMENTATION_
#include "xs/xs_logger.h"
#include "xs/xs_server.h"
#include "xs/xs_connection.h"
#include "xs/xs_json.h"
#undef _xs_IMPLEMENTATION_
