#ifdef WIN32
#include <ws2tcpip.h> //if you want IPv6 on Windows, this has to be first
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "xs/xs_types.h"
#include "xs/xs_logger.h"
#include "xs/xs_server.h"
#include "xs/xs_connection.h"

#ifdef WIN32
#define sleep Sleep
#endif

int main(int argc, char *argv[]) {
    int err=0, accesslog=0;
    xs_server_ctx *ctx;

    //parse command line
    if (argc>1 && strcmp(argv[1],"-a")==0) {
        printf ("access log on\n");
        accesslog = 1;
    } 

    //start server and connections
    xs_server_init_all (1);
    ctx = xs_server_create(".", argv[0]);
    if (accesslog==0) xs_logger_level(exs_Log_Error, exs_Log_Info);

    //main server loop
	xs_server_listen     (ctx, 8080);
	xs_server_listen_ssl (ctx, 443, "default_webxs_ssl_key.pem", "default_webxs_ssl_key.pem", "default_webxs_ssl_key.pem");
	while (xs_server_active(ctx)) {sleep(1);}
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
