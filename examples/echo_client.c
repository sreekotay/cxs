#include <netinet/tcp.h>
#include <errno.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "xs/xs_types.h"
#include "xs/xs_logger.h"
#include "xs/xs_server.h"
#include "xs/xs_ssl.h"
#include "xs/xs_connection.h"
#include "xs/xs_fileinfo.h"
#include "xs/xs_json.h"
#include "xs/xs_arr.h"
#include "xs/xs_printf.h"
#include "xs/xs_posix_emu.h"

char read_buffer[1024];
int bytes_read;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

typedef enum 
{
    ConnectionState_Disconnected,
    ConnectionState_Connecting,
    ConnectionState_Connected,
    ConnectionState_ShuttingDown,
    ConnectionState_Shutdown
} State;

State state = ConnectionState_Disconnected;

void wait_for(State s, xs_conn* conn)
{
    pthread_mutex_lock(&mutex);
    while (state != s)
        pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

void set_state(State s)
{
    pthread_mutex_lock(&mutex);
    state = s;
    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond);
}

int port = 443;
char* host = "echo.websocket.org";
char* path = "/";

int mycb(struct xs_async_connect* msg, int message, void* messageData, xs_conn* conn) 
{
#define LOGCASE(C) case C: printf("%lu: %s\n", pthread_self(), #C); break

    switch (message)
    {
        case exs_Conn_New:
            printf("%lu -- exs_Conn_New\n", pthread_self());
            break;

        case exs_Conn_Close:
            printf("%lu -- exs_Conn_Close\n", pthread_self());
            set_state(ConnectionState_ShuttingDown);
            break;

        case exs_XAS_Destroy:
            printf("%lu -- exs_XAS_Destroy\n", pthread_self());
            set_state(ConnectionState_Shutdown);
            break;

//        LOGCASE(exs_XAS_Create);
//        LOGCASE(exs_Conn_Read);
        LOGCASE(exs_Conn_Error);
        LOGCASE(exs_Conn_Handled);
        LOGCASE(exs_Conn_Write);
        LOGCASE(exs_Conn_Idle);

        default:
        //printf("%lu -- unhandled message: %d\n", pthread_self(), message);
        break;
    }

    int readMore = 1;
    int err = 0;

    while (readMore && message == exs_Conn_Read && err == 0)
    {
        int n = xs_conn_httpread(conn, read_buffer + bytes_read, sizeof(read_buffer) - bytes_read, &readMore);
        if (n > 0) 
            bytes_read += n;

        int s = xs_conn_state(conn);
        err = xs_conn_error(conn);

        switch (s)
        {
            case exs_Conn_WSNew:
                // printf("%lu -- exs_Conn_WSNew\n", pthread_self());
                break;

            case exs_Conn_WSFrameBegin:
                // printf("%lu -- exs_Conn_WSFrameBegin: %d\n", pthread_self(), n);
                break;

            case exs_Conn_WSFrameRead:
                // printf("%lu -- exs_Conn_WSFrameRead\n: %d\n", pthread_self(), n);
                break;

            case exs_Conn_WSFrameEnd:
                // printf("exs_Conn_WSFrameEnd: %d\n", bytes_read);
                // This seems to be the correct way to figure out when the websocket is
                // connected.
                pthread_mutex_lock(&mutex);
                if (state == ConnectionState_Connecting) {
                    state = ConnectionState_Connected;
                    pthread_cond_signal(&cond);
                }
                pthread_mutex_unlock(&mutex);

                if (bytes_read > 0 && state == ConnectionState_Connected) {
                    //printf("opcode:%d\n", xs_http_getint(xs_conn_getreq(conn), exs_Req_Opcode));
                    printf("recv: \"%.*s\"\n", bytes_read, read_buffer);
                    bytes_read = 0;
                }
                break;

            case exs_Conn_Pending:
                // printf("%lu -- eexs_Conn_Pending:%d\n", pthread_self(), n);
                break;

            default:
                if (n < 0)
                    printf("err: %d\n", xs_conn_error(conn));
                printf("unknown state %d: n=%d\n", s, n);
                break;
        }
    }
}


int main() 
{
    int err = 0;
    int i = 0;
    char msg[256];

    xs_SSL_initialize();

    bytes_read = 0;
    state = ConnectionState_Connecting;

    xs_conn* conn = NULL;
    xs_async_connect* xas = NULL;

    err = xs_conn_open(&conn, host, port, 1);
    if (err != 0)
    {
        printf("connect failed: %d\n", err);
        return 0;
    }
    
    xas = xs_async_read(xas, conn, mycb);
    xs_conn_httpwebsocket(conn, host, "/");
    xs_conn_header_done(conn, 0);

    // wait for connection
    wait_for(ConnectionState_Connected, conn);

    for (i = 0; i < 10; ++i)
    {
        memset(msg, 0, sizeof(msg));
        sprintf(msg, "hello world: %ld", time(0));
        printf("send: \"%s\"\n", msg);
        xs_conn_write_websocket(conn, exs_WS_PING, msg, strlen(msg), 0);
        sleep(1);
    }

    wait_for(ConnectionState_ShuttingDown, conn);

    xs_async_stop(xas);
    wait_for(ConnectionState_Shutdown, conn);

    xs_conn_destroy(conn);
    xs_async_stop(xas);

    wait_for(ConnectionState_Shutdown, conn);
    xs_conn_destroy(conn);
    xs_async_destroy(xas);

    xs_SSL_uninitialize();

    return 0;
}

#define _xs_IMPLEMENTATION_
#include <assert.h>
#include "xs/xs_queue.h"
#include "xs/xs_connection.h"
#include "xs/xs_fileinfo.h"
#include "xs/xs_logger.h"
#include "xs/xs_socket.h"
#include "xs/xs_ssl.h"
#undef _xs_IMPLEMENTATION_

