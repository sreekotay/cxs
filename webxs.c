#ifdef WIN32
    #ifndef FD_SETSIZE
    #define FD_SETSIZE 1024
    #endif
    #include <ws2tcpip.h>   //if you want IPv6 on Windows, this has to be first
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "xs/xs_types.h"
#include "xs/xs_logger.h"
#include "xs/xs_server.h"
#include "xs/xs_connection.h"

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
            while (err==0 && rr) {
                n = xs_conn_httpread(conn, buf, sizeof(buf)-1, &rr);
                err = xs_conn_error (conn);
                if (n>0 && websocket_data_handler (conn, buf, n)==0)
                    xs_conn_write_websocket (conn, exs_WS_CONNECTION_CLOSE, 0, 0, 0);
            }
            break;

        case exs_Conn_HTTPNew:
            printf ("conn %d\n", (int)xs_atomic_inc(gcount));
            break;
    }
    return xs_async_defaulthandler (xas, message, conn);
}

int main(int argc, char *argv[]) {
    int err=0, accesslog=0;
    xs_server_ctx *ctx;
    xs_async_connect* xas;
    char sslkey[] = "default_webxs_ssl_key.pem";

    //parse command line
    if (argc>1 && strcmp(argv[1],"-a")==0) {
        printf ("access log on\n");
        accesslog = 1;
    } 

    //start server and connections
    xs_server_init_all (1);
    ctx = xs_server_create(".", argv[0]);
    xas = xs_server_xas(ctx); 
    if (accesslog==0) xs_logger_level(exs_Log_Error, exs_Log_Info);

    //main server loop
	xs_server_listen     (ctx, 8080, myhandler);
	xs_server_listen_ssl (ctx, 443,  myhandler, sslkey, sslkey, sslkey);
	while (xs_server_active(ctx)) {
        switch (getc(stdin)) {
            case 's': xs_async_print(xas); break;
        }
    }
	xs_logger_fatal ("---- done ----");

    //all done
    xs_server_terminate_all(0,0);
    xs_logger_counter_print();
    return 7734;
}





//required libraries
#define _xs_IMPLEMENTATION_
#include "xs/xs_logger.h"
#include "xs/xs_server.h"
#include "xs/xs_connection.h"
#undef _xs_IMPLEMENTATION_
