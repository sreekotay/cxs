// =================================================================================================================
// xs_connection.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_CONNECTION_H_
#define _xs_CONNECTION_H_

#include "xs_atomic.h"
#include "xs_socket.h"

//messages
enum {
    //callback messages
    exs_Conn_Error              = -1,
    exs_Conn_Handled            = 1,
    exs_Conn_New                = 1000,
    exs_Conn_Read,
    exs_Conn_Write,
    exs_Conn_Close,
    exs_Conn_Idle,

    exs_Conn_HTTPNew,
    exs_Conn_HTTPRead,
    exs_Conn_HTTPComplete,

    exs_Conn_WSNew,
    exs_Conn_WSRead,
    exs_Conn_WSComplete,

    //connection states
    exs_Conn_Pending,
    exs_Conn_Response,
    exs_Conn_Redirect,
    exs_Conn_Complete,

    //attribute enums
    exs_Req_Method              = 1100,
    exs_Req_Version,
    exs_Req_URI,
    exs_Req_Query,
    exs_Req_ContentLen,
    exs_Req_Status,
    exs_Req_KeepAlive,
    exs_Req_Upgrade,
    exs_Req_Opcode, //websocket

    //websocket opcodes
    exs_WS_CONTINUATION         = 0x0,
    exs_WS_TEXT                 = 0x1,
    exs_WS_BINARY               = 0x2,
    exs_WS_CONNECTION_CLOSE     = 0x8,
    exs_WS_PING                 = 0x9,
    exs_WS_PONG                 = 0xa,

    //limits
    exs_Max_Headers             = 64,

    //error codes
    exs_Error_InvalidRequest    = -20000,
    exs_Error_HeaderTooLarge    = -20001,
    exs_Error_OutOfMemory       = -20002,
    exs_Error_InvalidSocket     = -20003,
    exs_Error_BindFail          = -20004,
    exs_Error_WriteBusy         = -20005,
    exs_Error_BadRedirect       = -20006,
    exs_Error_NoAutoRedirect    = -20007,
    exs_Error_WS_BadOpcode      = -20008,
    exs_Error_WS_BadContinue    = -20009,
    exs_Error_WS_InvalidSecKey  = -20010,
    exs_Error_Last
};



// ======================================================================
//  xs_conn
// ======================================================================
typedef struct xs_conn xs_conn;
typedef struct xs_httpreq xs_httpreq;

//manage
int                 xs_conn_listen          (xs_conn**, int port, int use_ssl, int ipv6);
int                 xs_conn_open            (xs_conn**, const char* host, int port, int use_ssl);
int                 xs_conn_opensock        (xs_conn**, int sock, int use_ssl);
int                 xs_conn_error           (const xs_conn*);
xs_conn*            xs_conn_destroy         (xs_conn*);
xs_conn*            xs_conn_inc             (xs_conn*); 
xs_conn*            xs_conn_dec             (xs_conn*); 
xs_atomic           xs_conn_seq             (xs_conn*); 
xs_atomic           xs_conn_seqinc          (xs_conn*); 
xs_atomic           xs_conn_rcount          (xs_conn*); 

//write functions
size_t              xs_conn_header_done     (xs_conn*, int hasBody); //very important
void                xs_conn_body_done       (xs_conn*);
char                xs_conn_writable        (xs_conn*);
char                xs_conn_writeblocked    (xs_conn*);
size_t              xs_conn_write           (xs_conn*, const void* buf, size_t len);
size_t              xs_conn_write_          (xs_conn*, const void* buf, size_t len, int flags);
size_t              xs_conn_write_header    (xs_conn*, const void* buf, size_t len);
size_t              xs_conn_write_chunked   (xs_conn*, const void* buf, size_t len);
int                 xs_conn_printf          (xs_conn*, const char *fmt, ...);
int                 xs_conn_printf_header   (xs_conn*, const char *fmt, ...);
int                 xs_conn_printf_chunked  (xs_conn*, const char *fmt, ...);
int                 xs_conn_printf_va       (xs_conn*, const char *fmt, va_list apin);
int                 xs_conn_printf_header_va(xs_conn*, const char *fmt, va_list apin);
int                 xs_conn_write_websocket (xs_conn*, int opcode, const char *data, size_t len, char do_mask);
size_t              xs_conn_write_httperror (xs_conn*, int statuscode, const char* description, const char* body, ...);

//http request functions                    //note: you must call xs_conn_header_done() afterwards to complete request
int                 xs_conn_httprequest     (xs_conn*, const char* host, const char* method, const char* path); //host=0 is valid -- will use conn host if present
int                 xs_conn_httpwebsocket   (xs_conn*, const char* host, const char* path); //host=0 is valid -- will use conn host if present
int                 xs_conn_followredirect  (xs_conn** reconn, xs_conn* conn, const char* method);

//read functions
size_t              xs_conn_read            (xs_conn*, void* buf, size_t len, int* reread);
size_t              xs_conn_httpread        (xs_conn*, void* buf, size_t len, int* reread);

//connection metadata
int                 xs_conn_setuserdata     (xs_conn*, void* data);
void*               xs_conn_getuserdata     (const xs_conn*);
int                 xs_conn_getsock         (const xs_conn*);
const xs_sockaddr*  xs_conn_getsockaddr     (const xs_conn*);
const char*         xs_conn_getsockaddrstr  (const xs_conn*);
int                 xs_conn_state           (const xs_conn*);
void*               xs_conn_sslctx          (const xs_conn*);

//request metadata
xs_httpreq*         xs_conn_getreq          (const xs_conn*);
const char*         xs_http_get             (const xs_httpreq*, int attr_name);
int                 xs_http_getint          (const xs_httpreq*, int attr_name);
int                 xs_http_setint          (      xs_httpreq*, int attr_name, int val);
const char*         xs_http_getheader       (const xs_httpreq*, const char* header);

// ======================
// URI utilities
// ======================
typedef struct xs_uri {
    int port;
    char* protocol;
    char* host;
    char* path;
    char* query;
    char* hash;
} xs_uri;
int                 xs_uri_decode           (char* dst, int dlen, const char* src, int slen, char formencoded); //OK if src==dst
int                 xs_uri_encode           (char* dst, int dlen, const char* src, int slen, char formencoded); //NOT OK if src==dst
xs_uri*             xs_uri_create           (const char* str, int forceHost);
xs_uri*             xs_uri_destroy          (xs_uri* uri);

const char*         xs_server_name          ();


// ======================================================================
//  xs_async
// ======================================================================
typedef struct xs_async_connect xs_async_connect;
typedef int (xs_async_callback) (struct xs_async_connect* xas, int message, xs_conn* conn);

int                 xs_async_defaulthandler (struct xs_async_connect* xas, int message, xs_conn* conn);
xs_async_connect*   xs_async_create         (int hintsize);
xs_async_connect*   xs_async_read           (xs_async_connect*, xs_conn*, xs_async_callback* proc); //xs_async_connect may be null
xs_async_connect*   xs_async_listen         (xs_async_connect*, xs_conn*, xs_async_callback* proc); //xs_async_connect may be null
xs_async_connect*   xs_async_destroy        (xs_async_connect* );
int                 xs_async_call           (xs_async_connect* xas, int message, xs_conn* conn);
void                xs_async_stop           (xs_async_connect* );
int                 xs_async_active         (xs_async_connect* );
int                 xs_async_print          (xs_async_connect* );

//helper functions
void                xs_async_setuserdata    (xs_async_connect*, void* );
void*               xs_async_getuserdata    (xs_async_connect*);


// =================================================================================================================
// =================================================================================================================
#endif //_xs_CONNECT_H_




//leftover
#ifndef MSG_MORE
#define MSG_MORE        MSG_PARTIAL
#endif //MSG_MORE













// =================================================================================================================
//  implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_CONNECTION_IMPL_
#define _xs_CONNECTION_IMPL_

#undef _xs_IMPLEMENTATION_
#include "xs_types.h"
#include "xs_arr.h"
#include "xs_socket.h"
#include "xs_ssl.h"
#include "xs_printf.h"
#include "xs_sha1.h"
#include "xs_logger.h"
#define _xs_IMPLEMENTATION_


// =================================================================================================================
//  conn implementation
// =================================================================================================================
struct xs_conn {
    xs_atomic           refcount, seq, rcount;     
    unsigned int        is_ssl:8, corked:8, is_wblocked:2, upgrade:4, proto:2;
    int                 sock;
    int                 errnum;
    xs_sockaddr         sa;
    int                 port;
    int                 datalen;
    int                 consumed;
    int                 bufsize;
    char*               host;
    char*               scratch;
    char*               buf;
    char                sslproto[10];
    xs_httpreq          *req, *lastreq, *freereq;
    char                method[10];//max method;
#ifndef NO_SSL
    SSL_CTX*            sslctx;
    SSL*                ssl;
#endif
    xs_array            wcache;
    xs_async_callback*  cb;
    void*               userdata;
    char                sastr[46];
};

struct xs_httpreq {
    unsigned int        statuscode, chunked:1, keepalive:1, upgrade:4, opcode:4, done:2;
    int                 reqlen, reqmallocsize;
    char                datamask[4];
    time_t              createtime;
    size_t              contentlen;
    size_t              consumed;
    xs_httpreq*         next;
    const char          *uri, *method, *version, *status, *hash, *query;
    char*               buf;
    int                 numheaders;
    struct xs_httpheader {
        const char *name;       
        const char *value;      
    } headers[exs_Max_Headers];         
};

// ==============================================
//  conn mgmt
// ==============================================
int          xs_conn_error (const xs_conn* conn)                    {return conn ? conn->errnum : -50;}
int          xs_conn_getsock (const xs_conn* conn)                  {return conn ? conn->sock : INVALID_SOCKET; }
int          xs_conn_setuserdata    (xs_conn* conn, void* data)     {if (conn==0) return -50; conn->userdata=data; return 0;}
void*        xs_conn_getuserdata    (const xs_conn* conn)           {return conn ? conn->userdata : 0;}
const char*  xs_conn_getsockaddrstr (const xs_conn* conn)           {return conn ? conn->sastr : 0;}
const xs_sockaddr* xs_conn_getsockaddr (const xs_conn* conn)        {return conn ? &conn->sa : 0; }
             
int          xs_conn_cachepurge (xs_conn* conn); //forward declaration
xs_conn*     xs_conn_create()   {xs_logger_counter_add (1, 1); return (xs_conn*)calloc(sizeof(xs_conn), 1);}
xs_conn*     xs_conn_close(xs_conn *conn) {
    struct xs_httpreq *req, *hold;
    if (conn==0) return 0;
    if (xs_arr_count(conn->wcache)) {           //purge write cache
        xs_sock_setnonblocking(conn->sock, 1);  //blocking?  or non-blocking? $$$SREE
        xs_conn_cachepurge(conn);
        xs_sock_setnonblocking(conn->sock, 1);
    }

#ifndef NO_SSL
    if (conn->sslctx != NULL)
      xs_SSL_freeCTX ((SSL_CTX *)conn->sslctx);
#endif
    xs_arr_destroy(conn->wcache);
    if (conn->sock>0)   xs_sock_close(conn->sock);
    if (conn->buf)      free (conn->buf);
    if (conn->host)     free (conn->host);
    if (conn->scratch)  free (conn->scratch);
    req = conn->req;
    while (req) {
        hold = req;
        req = req->next;
        free (hold);
    }
    memset (((char*)conn)+sizeof(xs_atomic), 0, sizeof(*conn)-sizeof(xs_atomic));
    return conn;
}
xs_conn*    xs_conn_destroy(xs_conn *conn) {
    if (conn==0) return 0;
    xs_conn_close(conn);
    if (conn->refcount<=0) {
        free(conn);
        xs_logger_counter_add (1, -1);
    }
    return conn;
}
xs_conn*    xs_conn_inc(xs_conn *conn) {
    if (conn==0) return 0;
    xs_atomic_inc(conn->refcount);
    return conn;
}
xs_conn*    xs_conn_dec(xs_conn *conn) {
    if (conn==0) return 0;
    if (xs_atomic_dec(conn->refcount)<=1)   return xs_conn_destroy(conn);
    return conn;
}
xs_atomic   xs_conn_seq (xs_conn* conn) {
    if (conn==0) return 0;
    return conn->seq;
}
xs_atomic   xs_conn_rcount (xs_conn* conn) {
    if (conn==0) return 0;
    return conn->rcount;
}
xs_atomic   xs_conn_seqinc (xs_conn* conn) {
    if (conn==0) return 0;
    return xs_atomic_inc (conn->seq);
}

const char* xs_server_name()  {static const char name[]="webxs/0.5"; return name;}


// ==============================================
//  open
// ==============================================
int xs_conn_opensock_ (xs_conn** connp, int sock, int use_ssl) {
    int err = 0;
    socklen_t len = (int)sizeof(struct sockaddr);
    xs_conn* conn=*connp;
    if (conn==0 && (conn=xs_conn_create())==0) return -108;

    conn->sock = sock;
    xs_sock_closeonexec(sock);
    getsockname(conn->sock, &conn->sa.saddr, &len);
    xs_sock_addrtostr(conn->sastr, sizeof(conn->sastr), (const xs_sockaddr*)&conn->sa.saddr);


#ifndef NO_SSL
    conn->is_ssl = use_ssl;
    if (use_ssl && xs_SSL_ready() && (conn->sslctx=xs_SSL_newCTX_client())==0) err=-1000;
    if (err) {xs_conn_destroy(conn); return err;}

    if (use_ssl && xs_sslize_connect(&conn->ssl, conn->sslctx, conn->sock)) err = -1001;
    if (err) {xs_conn_destroy(conn); return err;}
#else
    if (use_ssl) err = -1000;
    if (err) {xs_conn_destroy(conn); return err;}
#endif

    *connp = conn;
    return 0;
}
int xs_conn_opensock (xs_conn** connp, int sock, int use_ssl) {
    if (connp==0) return -50; *connp=0;
    return xs_conn_opensock_(connp, sock, use_ssl);
}

int xs_conn_open_ (xs_conn** connp, const char* host, int port, int use_ssl) {
    int sock, err;
#ifndef NO_SSL
    if (use_ssl && xs_SSL_ready() == 0) return -10;
#else
    if (use_ssl) return -10;
#endif
    err = xs_sock_open (&sock, host, port, 1);
    if (err==0) err = xs_conn_opensock_ (connp, sock, use_ssl);
    if (err==0) {(*connp)->port = port; (*connp)->host = xs_strdup(host);}
    return err;
}
int xs_conn_open (xs_conn** connp, const char* host, int port, int use_ssl) {
    if (connp==0) return -50; *connp=0;
    return xs_conn_open_(connp, host, port, use_ssl);
}

// ==============================================
//  listen
// ==============================================
int xs_conn_listen_ (xs_conn** connp, int port, int use_ssl, int ipv6) {
    xs_conn* conn=*connp;
    int sockfd, err;
#ifdef NO_SSL
    if (use_ssl) return -50;
#endif
    err = xs_sock_listen(&sockfd, port, 1, ipv6);
    if (err) return err;
    if (conn==0 && (conn=xs_conn_create())==0) return -108;

    //success
    conn->sock = sockfd;
    conn->is_ssl = use_ssl;
    conn->port = port;
#ifndef NO_SSL
    if (use_ssl) {
        if (xs_SSL_ready()) conn->sslctx = xs_SSL_newCTX_server();
        if (conn->sslctx==0) {
            xs_conn_destroy(conn); return -51;}
    }
#endif
    *connp = conn;
    return 0;
}
int xs_conn_listen (xs_conn** connp, int port, int use_ssl, int ipv6) {
    if (connp==0) return -50; *connp=0;
    return xs_conn_listen_(connp, port, use_ssl, ipv6);
}

// ==============================================
//  accept
// ==============================================
int xs_conn_accept_ (xs_conn** connp, int sockfd, xs_sockaddr* sockaddr, void* sslctx) {
    xs_conn* conn=*connp;
    if (conn==0 && (conn=xs_conn_create())==0) return -108;

    conn->sock = sockfd;
    if (sockaddr)   {
        conn->sa = *sockaddr;
        xs_sock_addrtostr(conn->sastr, sizeof(conn->sastr), sockaddr);
    }
    
    conn->is_ssl = (sslctx!=0);
    if (sslctx && xs_sslize_accept (&conn->ssl, (SSL_CTX*)sslctx, sockfd)==0) {
        xs_conn_destroy(conn);
        return -1;
    }
    *connp = conn;
    return 0;
}
int xs_conn_accept (xs_conn** connp, int sockfd, xs_sockaddr* sockaddr, void* sslctx) {
    if (connp==0) return -50; *connp=0;
    return xs_conn_accept_(connp, sockfd, sockaddr, sslctx);
}


// ==============================================
//  metadata function
// ==============================================
int xs_conn_seterr (xs_conn* conn) {
    if (conn==0) return -1;
#ifdef WIN32
    conn->errnum = WSAGetLastError();
    if (conn->errnum==WSAEWOULDBLOCK) conn->errnum=0;
#else
    conn->errnum = errno;
    if (conn->errnum==EAGAIN || conn->errnum==EWOULDBLOCK) conn->errnum=0;
#endif
    return conn->errnum;
}

int xs_http_state (xs_httpreq *req) {
    if (req==0 || req->reqlen==0)                       return exs_Conn_Pending;
    if (req->contentlen && req->chunked==0 &&
        req->consumed == req->contentlen)               return exs_Conn_Complete;
    if (req->contentlen || req->chunked)                return exs_Conn_Response;
    if (req->statuscode>=300 && req->statuscode<=303)   return exs_Conn_Redirect;
    return exs_Conn_Complete;
}

int xs_conn_state (const xs_conn *conn) {
    if (conn==0 || conn->sock<=0 || conn->errnum)   return exs_Conn_Error;
    return xs_http_state(conn->req);
}

xs_httpreq* xs_conn_getreq  (const xs_conn* conn) {xs_httpreq* req=conn?conn->req:0; while(req&&req->done) req=req->next; return req;}
void*       xs_conn_sslctx  (const xs_conn* conn) {return conn ? conn->sslctx : 0;}

const char *xs_http_get (const xs_httpreq* req, int attr) {
    if (req)
    switch (attr) {
        case exs_Req_Version:       return req->version;
        case exs_Req_Method:        return req->method;
        case exs_Req_URI:           return req->uri;
        case exs_Req_Query:         return req->query;
        case exs_Req_Status:        return req->status;
    }
    return 0;
}


int xs_http_getint (const xs_httpreq* req, int attr) {
    if (req)
    switch (attr) {
        case exs_Req_Status:        return req->statuscode;
        case exs_Req_ContentLen:    return req->contentlen;
        case exs_Req_KeepAlive:     return req->keepalive;
        case exs_Req_Upgrade:       return req->upgrade;
        case exs_Req_Opcode:        return req->opcode;
    }
    return 0;
}

int xs_http_setint (xs_httpreq* req, int attr, int val) {
    if (req)
    switch (attr) {
        case exs_Req_Status:        req->statuscode = val; break;
        case exs_Req_KeepAlive:     req->keepalive  = val; break;
        case exs_Req_Upgrade:       req->upgrade    = val; break;
        default:                    return -1;
    }
    return 0;
}

void xs_req_clear(xs_httpreq* req) {
    if (req==0) return;
    req->consumed = req->contentlen = 0;
    req->numheaders = req->opcode = req->done = req->statuscode = 0;
    req->chunked = req->upgrade = req->keepalive = 0;
    req->version = req->method = req->uri = req->query = req->status = 0;
    req->next = 0;
}

const char *xs_http_getheader(const xs_httpreq* req, const char *name) {
    int i;
    if (req==0) return 0;
    for (i=0; i<req->numheaders; i++)
      if (!xs_strcmp_case(name, req->headers[i].name))
        return req->headers[i].value;
    
    return 0;
}

int xs_conn_bufresize(xs_conn* conn) {
    if (conn->buf==0) {
        conn->buf = (char*)malloc (1024);
        conn->bufsize = 1024;
    } else  if (conn->consumed>=conn->datalen) {
        conn->datalen = 0;
        conn->consumed = 0;
    } else if (conn->consumed>(conn->bufsize>>2)*3) {
        memcpy (conn->buf, conn->buf+conn->consumed, conn->datalen-conn->consumed);
        conn->datalen -= conn->consumed;
        conn->consumed = 0;
    }
    else if (conn->datalen==conn->bufsize) {
        int bsize = conn->datalen*2;
        char* b = (char*)realloc(conn->buf, bsize);
        if (b) {conn->buf=b; conn->bufsize=bsize;}
    }
    return 0;
}

size_t xs_conn_write_httperror (xs_conn* conn, int statuscode, const char* description, const char* body, ...) {
    va_list ap_copy, apin;
    const char* ver;
    int result = 0;//, sock = 0, wt=0;
    va_start(apin, body);
    if (body) {
        va_copy(ap_copy, apin);
        result = xs_sprintf_va (0, 0, body, ap_copy);
        va_end (ap_copy);
    }
    ver = xs_http_get(xs_conn_getreq(conn), exs_Req_Version);
    if (ver && !strcmp(ver, "1.0")) ver=0;
    //if (wt&&xs_conn_writable(conn)==0) xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);
    xs_conn_printf_header (conn, 
        "HTTP/%s %d %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: %s\r\n"
        "Server:%s\r\n"
        "Date: %s\r\n",
        ver ? ver : "1.0", statuscode, description, result, 
        (ver&&xs_http_getint(xs_conn_getreq(conn), exs_Req_KeepAlive)) ? "keep-alive" : "close",
        xs_server_name(), xs_timestr_now());
    //if (wt&&xs_conn_writable(conn)==0) xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);
    xs_conn_header_done(conn, body && *body);

    if (body && *body) {
        va_copy(ap_copy, apin);
        //if (wt&&xs_conn_writable(conn)==0) xs_sock_setnonblocking(sock=xs_conn_getsock(conn), 0);
        result = xs_conn_printf_va (conn, body, ap_copy);
        va_end (ap_copy);
    }
    xs_http_setint (xs_conn_getreq(conn), exs_Req_Status, statuscode);
    //if (sock) xs_sock_setnonblocking(sock, 1);
    return result;
}



// ==============================================
//  websocket handling
// ==============================================
int xs_websocket_response(xs_conn *conn) {
    const char ws_magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const char *host, *upgrade, *connection, *version, *key;
    char buf[128], sha[21], shab64[sizeof(sha)<<1];
    SHA1_CTX sha_ctx;
    xs_httpreq* req = xs_conn_getreq(conn);
    if (req==0 || (key=xs_http_getheader(req, "Sec-WebSocket-Key"))==0) return -1;
   
    //check headers
    version = xs_http_getheader(req, "Sec-WebSocket-Version");
    if (version==0) return 2;

    host        = xs_http_getheader(req, "Host");
    upgrade     = xs_http_getheader(req, "Upgrade");
    connection  = xs_http_getheader(req, "Connection");

    if (host==0 || upgrade==0 || connection==0) return 3;
    if (xs_strstr_case(upgrade, "websocket")==0 ||
        xs_strstr_case(connection, "Upgrade")==0) return 4;

    if (strcmp(version, "13")!=0) {
        xs_conn_write_httperror (conn, 425, "Upgrade Required\r\nSec-WebSocket-Version: 13", "Unsupported Websockets Version.");
        return 0;
    }

    //must be a GET
    if (xs_strcmp_case(req->method, "GET")) return exs_Error_InvalidRequest;

    //sha key + magic (base-64 encoded)
    xs_strlcpy      (buf, key, sizeof(buf));
    xs_strlcat      (buf, ws_magic, sizeof(buf)); 
    SHA1Init        (&sha_ctx);
    SHA1Update      (&sha_ctx, (unsigned char*)buf, (unsigned int)strlen(buf));
    SHA1Final       ((unsigned char*)sha, &sha_ctx);
    xs_b64_encode   (shab64, (unsigned char *)sha, 20);
    xs_conn_printf_header  (conn,   
                        "HTTP/1.1 101 Switching Protocols\r\n"
                        "Upgrade: websocket\r\n"
                        "Server: %s\r\n"
                        "Date: %s\r\n"
                        "Connection: Upgrade\r\n"
                        "Sec-WebSocket-Accept: %s\r\n",
                        xs_server_name(),
                        xs_timestr_now(),
                        shab64);
    xs_conn_header_done(conn, 0);
    conn->upgrade = 2; //upgraded!
    return 0;
}

int xs_conn_httpwebsocket (xs_conn* conn, const char* host, const char* path) {
    char sec_key[]="sr33k0t2ypZYKUwIrsGYaw==";
    if (conn==0 || path==0) {if (conn) conn->errnum=exs_Error_InvalidRequest; return -50;}
    if (host==0) host = conn->host;
    xs_conn_printf_header (conn, 
                    "GET %s HTTP/1.1\r\n"
                    "Upgrade: websocket\r\n"
                    "Server: %s\r\n"
                    "Date: %s\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    "Sec-WebSocket-Key: %s\r\n"
                    "%s%s%s",
                    path, xs_server_name(), xs_timestr_now(),
                    sec_key,
                    host ? "Host: " : "",
                    host ? host : "",
                    host ? "\r\n" : ""
                    );
    return 0;
}

int xs_websocket_accept(xs_conn* conn) {
    xs_httpreq* req = conn ? xs_conn_getreq(conn) : 0;
    char sec_accept[] = "zY5/M7Ht2MENvjW5tL57/BmNnE4="; //base64 encoded SHA1 hash of sec_key+258EAFA5-E914-47DA-95CA-C5AB0DC85B11
    const char* h = xs_http_getheader (req, "Sec-WebSocket-Accept");
    if (h==0 || xs_strcmp(h, sec_accept))   return -1;
    if (conn) conn->upgrade = 2; //upgraded!
    return 0;
}

int xs_websocket_framevalid(const char *buf, int buflen, char maskptr[4], size_t* dlenptr) {
    int rlen = 0, len, masklen=0;
    size_t datalen=0;
    if (buflen >= 2) {
        len = buf[1]&127;
        masklen = (buf[1]&0x80) ? 4 : 0;
        if (len<126 && buflen>=masklen + 2) {
            rlen    = masklen + 2;
            datalen = len;
        } else if (len == 126 && buflen >= masklen + 4 ) {
            rlen    = masklen + 4;
            datalen = ntohs(*(short *)&buf[2]);
        } else if (buflen >= masklen + 10) {
            rlen    = masklen + 10;
            datalen = (size_t)(((xsuint64)ntohl(*(xsuint32 *)&buf[2]))<<32) + ntohl(*(xsuint32 *)&buf[6]);
        }
    }

    if (dlenptr) *dlenptr = datalen;
    if (maskptr) {if (masklen && rlen) memcpy(maskptr, &buf[rlen-4], 4); else memset (maskptr, 0, 4);}
    return rlen;
}

int xs_websocket_frameparse (xs_httpreq* req, char* buf, char datamask[4], size_t contentlen) {
    int err=0, opc;
    if (req==0) return -1;
    //memset (req, 0, sizeof(*req));
    xs_req_clear (req);
    req->contentlen = contentlen;
    memcpy (req->datamask, datamask, 4);
    req->keepalive = (((opc=buf[0]&0x0f))!=exs_WS_CONNECTION_CLOSE);   //keepalive based on close connecion websockets opcode
    if ((opc>2 && opc<8) || opc>10)      err = exs_Error_WS_BadOpcode;
    else if (opc==0 && req->opcode==0)   err = exs_Error_WS_BadContinue;
    if (opc || err)                      req->opcode = opc;
    return err;
}

static void xs_ws_genmask(unsigned char mask[4], void* genptr) { //mask MUST be of size 4
    //very poor pseudo random number for masking -- uses rotating address of data ptr
    static int rot=0; 
    int data = (int)((size_t)(genptr));       rot++;
    mask[(rot+0)&3] = (xsuint8)(data>>0);     mask[(rot+1)&3] = (xsuint8)(data>>8);
    mask[(rot+2)&3] = (xsuint8)(data>>16);    mask[(rot+3)&3] = (xsuint8)(data>>24);
}

int xs_conn_write_websocket (xs_conn *conn, int opcode, const char *data, size_t len, char do_mask) {
    unsigned char mask[4];
    unsigned char hdr[14], buf[4096];
    int hlen, retval, i, outlen;
    if (data==0 || len==0) {data=0; len=0;}

    hdr[0] = 0x80 + (opcode&0xF);
    if (len < 126) {
        //7-bit
        hdr[1] = (xsuint8)len;
        hlen = 2;
    } else if (len <= 0xFFFF) { 
        //16-bit
        hdr[1] = 126;
        *(xsuint16*)(hdr + 2) = htons((unsigned short)len);
        hlen = 4;
    } else { 
        //64-bit
        hdr[1] = 127;
        *(xsuint32*)(hdr + 2) = htonl(((xsuint64)len)>>32);
        *(xsuint32*)(hdr + 6) = htonl((len)&0xffffffff);
        hlen = 10;
    }

    if (do_mask && len) {
        hdr[1] |= 0x80;              //masking bit
        xs_ws_genmask               (mask, (void*)data);
        for (i=0; i<4; i++)          hdr[hlen++] = mask[i];
        retval  = xs_conn_write_(conn, hdr, hlen, MSG_MORE); 
        while (len) {
            if (len>sizeof(buf))    outlen = (int)sizeof(buf);
            else                    outlen = (int)len;
            memcpy (buf, data, outlen);
            for (i=0; i<outlen; i++) buf[i]^=mask[i&3]; //should we optimize with Duff's device?
            retval += xs_conn_write_ (conn, buf, outlen, len>(size_t)outlen ? MSG_MORE : 0);
            len -= outlen;
        }
    } else {
        retval  = xs_conn_write_(conn, hdr, hlen, len ? MSG_MORE : 0); 
        retval += xs_conn_write (conn, data, len);
    }

    //write it
    return retval;
}

// ==============================================
//  http parsing
// ==============================================
static int xs_scan_chunk (const char *buf, int buflen, const char** ret) {
    const char *s, *e;
    int datalen=0, gotdata=0, v;
    
    for (s=buf, e=s+buflen-1; s<e; s++) {
        if (gotdata && s[0]=='\r' && s[1]=='\n') {
            *ret=s+2+(datalen?0:2); 
            return datalen;
        }
        if (gotdata>1) continue;
        v = xs_fromhex(s[0]);
        if (v>=0) {
            datalen=datalen*16+v;
            gotdata=1;
        } else if (gotdata) gotdata++;
    }
    *ret=0;
    return -1;
}

int xs_http_okchar(int a) {
    return a=='\r'||a=='\n'||a=='\t'||(unsigned int)(a-32)<(unsigned int)(127-32); //not \r or \n, but >=32 and <127
} 

static int xs_http_skipspace(const char* buf, int rlen, int c) {
    while (c<rlen && (buf[c]=='\t'||buf[c]==' ')) c++;
    return c;
}
static int xs_http_skiptokenandspace(const char* buf, int rlen, int c) {
    while (c<rlen && (buf[c]=='\t'||buf[c]==' '||buf[c]=='\r'||buf[c]=='\n')==0) c++;
    while (c<rlen && (buf[c]=='\t'||buf[c]==' '))    c++;
    return c;
}
static int xs_http_skiptoken(const char* buf, int rlen, int c) {
    while (c<rlen && (buf[c]=='\t'||buf[c]==' ')==0) c++;
    return c;
}
static int xs_http_skiptoEOL(const char* buf, int rlen, int c) {
    while (c<rlen-1 && buf[c]!='\r' && buf[c+1]!='\n') c++; //get to EOL
    return c;
}
static int xs_http_terminatetoken (char* buf) {
    int c=0; if (buf==0) return 0;
    while ((buf[c]=='\r'||buf[c]=='\n'||buf[c]=='\t'||buf[c]==' ')==0) c++;
    buf[c++]=0;
    return c;
}
char xs_http_validmethod(const char *method) {
    return  !strcmp(method, "GET")      || !strcmp(method, "POST") ||
            !strcmp(method, "PUT")      || !strcmp(method, "DELETE") || 
            !strcmp(method, "PROPFIND") || !strcmp(method, "CONNECT") ||
            !strcmp(method, "HEAD")     || !strcmp(method, "OPTIONS") ||
            !strcmp(method, "MKCOL");
}

static int xs_http_parseheaders (xs_httpreq* req, char* buf, int rlen) {
    const char* h; char *ch, *na, *va;
    int c=0, n=req->numheaders, err=0;
    while (n<exs_Max_Headers && c<rlen && err==0) {
        while (buf[c]<=32 && c<rlen) c++;   //skip whitespace, include '\r\n'
        if (c>=rlen) break;

        //get key
        req->headers[n].name = na = buf + c;
        c = xs_http_skiptoken (buf, rlen, c);
        if (buf[c-1]!=':')  {err=-10; break;}
        buf[c-1] = 0; //terminate

        //get value
        c = xs_http_skipspace (buf, rlen, c);
        req->headers[n].value = va = buf + c;
        c = xs_http_skiptoEOL (buf, rlen, c);
        buf[c] = 0; //terminate

        //pre-parse some headers
        h = xs_strcmp_case (na, "Content-Length") ? 0 : va;
        if (h) req->contentlen = (size_t)strtod (h, &ch);
    
        h = xs_strcmp_case (na, "Transfer-Encoding") ? 0 : va;
        if (h && !xs_strcmp_case(h, "chunked")) req->chunked = 1;

        h = xs_strcmp_case (na, "Connection") ? 0 : va;
        if (h && !xs_strcmp_case(h, "keep-alive")) req->keepalive = 1;      //this is 'else', because keep alive is the...
        else if (!xs_strcmp_case(req->version, "1.1")) req->keepalive = 1;  //...default for http 1.1, unless explicitly set

        h = xs_strcmp_case (na, "Upgrade") ? 0 : va;
        if (h) req->upgrade = 1; //upgrade connection to websocket (or http 2.0?)

        //next
        n++;
    }
    req->numheaders = n;
    return 0;
}

static int xs_http_parse(xs_httpreq* req, const char* buf, int rlen) {
    char *b, *ch;
    int c = xs_http_skipspace(buf, rlen, 0), ulen;
    //memset (req, 0, sizeof(*req));
    xs_req_clear(req);
    if (c>=rlen) return -50;
    
    //copy buffer
    req->buf = (char*)(req+1);
    if (req->buf==0) return -108; //not possible :P
    memcpy (req->buf, buf+c, rlen-c);
    req->buf[rlen-=c] = 0; //terminate
    c = 0;
    b = req->buf;

    //three parts to http response
    req->method = b+c;      c = xs_http_skiptokenandspace(b, rlen, c);
    req->uri = b+c;         c = xs_http_skiptokenandspace(b, rlen, c);
    req->version = b+c;     c = xs_http_skiptoEOL(b, rlen, c);
    req->status = req->hash = req->query = 0;

    //client request:       GET / HTTP/1.0 
    //request response:     HTTP/1.0 200 OK
    if (!memcmp(req->version, "HTTP/", 5))     {
        //its a client request
        req->version += 5; //skip to number
        xs_http_terminatetoken ((char*)req->method);
        if (xs_http_validmethod(req->method)==0) return -1; //valid method
        if (req->uri[0]!='/' && (req->uri[0]!='*' && !xs_http_okchar(req->uri[1]))) return -2; //valid URI
    }
    else if (!memcmp(req->method, "HTTP/", 5)) {
        //re-order because its a response
        req->status = req->version; 
        req->statuscode = (int)strtod (req->uri, &ch);
        req->version = req->method + 5;
        req->method = req->uri = 0;
    } else {
        req->method = req->uri = req->version = 0;
        return -3; //invalid request
    }

    //terminate stuff
    xs_http_terminatetoken ((char*)req->status);
    xs_http_terminatetoken ((char*)req->version);
    if (req->uri) {
        ulen = xs_http_terminatetoken ((char*)req->uri);
        ch = strchr((char*)req->uri, '#'); if (ch) {*ch++=0; req->hash=ch;  ulen = req->uri-ch; }
        ch = strchr((char*)req->uri, '?'); if (ch) {*ch++=0; req->query=ch; ulen = req->uri-ch; }
        xs_uri_decode((char*)req->uri, ulen, req->uri, ulen, 0);
        if (req->hash)  {ulen=strlen(req->hash) +1; xs_uri_decode((char*)req->hash,  ulen, req->hash,  ulen, 0);}
        if (req->query) {ulen=strlen(req->query)+1; xs_uri_decode((char*)req->query, ulen, req->query, ulen, 0);}
    }


    //skip to EOL of header and parse other headers
    return xs_http_parseheaders (req, b+c+1, rlen-c-1);
}

int xs_http_validrequest(const char *buf, int buflen) {
    const char *s=buf, *e=buf+buflen-1;
    int len = 0;
    while (s<e && len==0) {
        if (!xs_http_okchar(*s))                len = -1; //invalid character
        else if (s[0]=='\n' && s[1]=='\n')      len = (int)(s - buf) + 2;
        else if (s[0]=='\n' && s+1<e &&  
                 s[1]=='\r' && s[2]=='\n')      len = (int)(s - buf) + 3;
        s++;
    }
    return len;
}

int xs_http_createreq(xs_conn *conn, int rlen) { //rlen may be zero
    int err=0, reqmallocsize;
    char datamask[4]={0};
    size_t contentlen=0;
    xs_httpreq* req     = conn->freereq;
    if (conn->upgrade)  {rlen=0; req = conn->lastreq;}//re-parse for websockets
    else if (req)       {conn->freereq = req->next; req->next = 0;}

    //do we have a request?
    if (rlen<=0)        rlen = (conn->upgrade==0) ? xs_http_validrequest    (conn->buf, conn->datalen) :
                                                    xs_websocket_framevalid (conn->buf, conn->datalen, datamask, &contentlen);
    if (rlen<=0)        {conn->errnum = exs_Error_InvalidRequest; return rlen==0 ? -1 : -50;} //-1 = too big request, otherwise error

    //allocate space
    reqmallocsize                               = sizeof(xs_httpreq) + rlen + 1;
    if (req==0)                                 {req = (xs_httpreq*)malloc(reqmallocsize);             if (req) req->reqmallocsize = reqmallocsize;}
    else if (reqmallocsize>req->reqmallocsize)  {req = (xs_httpreq*)realloc(conn->req, reqmallocsize); if (req) req->reqmallocsize = reqmallocsize;}
    //else if (conn->upgrade==0)                  req = (xs_httpreq*)realloc(conn->req, reqmallocsize); //keep reusing if we're upgraded
    if (req==0)                     {conn->errnum = exs_Error_OutOfMemory; return -108;}
    if (conn->req==0)               {conn->req = req; assert (conn->lastreq==0);}
    else if (conn->lastreq)         conn->lastreq->next = req;
    conn->lastreq                   = req;
    
    //parse the http header
    err = (conn->upgrade==0) ?  xs_http_parse           (req, conn->buf, rlen) :
                                xs_websocket_frameparse (req, conn->buf, datamask, contentlen);
    if (err) {conn->errnum = exs_Error_InvalidRequest; return -2;}

    //mark the consumption
    req->reqlen = rlen;
    conn->consumed += rlen;
    req->createtime = time(0);
    return 0;
}

int xs_conn_headerready(xs_conn *conn) {
    int rlen=0, n=-1;
    int maxheaderlen = (1<<17);

    xs_conn_bufresize(conn);
    if (conn->datalen)
    rlen = (conn->upgrade==0) ? xs_http_validrequest    (conn->buf, conn->datalen) :
                                xs_websocket_framevalid (conn->buf, conn->datalen, 0, 0);
    if (rlen<0)                 conn->errnum = exs_Error_InvalidRequest;
    if (rlen==0 &&
        (n=xs_conn_read (conn, conn->buf + conn->datalen, conn->bufsize-conn->datalen, 0)) > 0) {
        conn->datalen += n;
        rlen = (conn->upgrade==0) ? xs_http_validrequest    (conn->buf, conn->datalen) :
                                    xs_websocket_framevalid (conn->buf, conn->datalen, 0, 0);
    }
    
    if (rlen>maxheaderlen || conn->datalen>maxheaderlen) conn->errnum = exs_Error_HeaderTooLarge;
    if (rlen>0) return rlen;
    return n==0 ? 0 : -1;
}

int xs_conn_chunkready(xs_conn * conn) {
    int n;
    xs_httpreq* req = xs_conn_getreq(conn);
    const char* enc;
    xs_conn_bufresize (conn);
    n = xs_scan_chunk (conn->buf + conn->consumed, conn->datalen-conn->consumed, &enc);
    if (n<0 && (n=xs_conn_read (conn, conn->buf + conn->datalen, conn->bufsize-conn->datalen, 0)) >= 0) {
        conn->datalen += n;
        n = xs_scan_chunk (conn->buf + conn->consumed, conn->datalen-conn->consumed, &enc);
    }
    if (n>=0) {
        //add to contentlen -- read data
        assert(enc && req);
        conn->consumed = (int)(enc - conn->buf);
        req->contentlen += n;
    } 
    return n;
}

// ==============================================
//  http request functioms
// ==============================================
int xs_conn_httprequest (xs_conn* conn, const char* host, const char* method, const char* path) {
    if (conn==0 || method==0 || path==0) {conn->errnum=exs_Error_InvalidRequest; return -50;}
    if (xs_http_validmethod(method)==0) {conn->errnum=exs_Error_InvalidRequest; return -1;}
    if (host==0) host = conn->host;
    xs_conn_printf_header (conn, 
                    "%s %s HTTP/1.1\r\n"
                    "Connection: keep-alive\r\n"
                    "Server: %s\r\n"
                    "Data: %s\r\n"
                    "%s%s%s",
                    method, path, xs_server_name(), xs_timestr_now(),
                    host ? "Host:" : "",
                    host ? host : "",
                    host ? "\r\n" : ""
                    );
    xs_strlcpy (conn->method, method, sizeof(conn->method));
    return 0;
}

int xs_conn_followredirect (xs_conn** reconn, xs_conn* conn, const char* method) {
    xs_uri* uri;
    const char* h;
    int err=0;
    xs_httpreq* req;
    if (conn==0 || reconn==0) return 0;
    req = conn->req;
    *reconn = 0;

    //check redirect
    h = xs_http_getheader(req, "Location");
    if (h==0) {conn->errnum = exs_Error_BadRedirect; return -1;}

    if ((uri=xs_uri_create(h, 0))==0 || //fail if no valid uri, or its not an understood uri
        (uri->protocol && xs_strcmp_case(uri->protocol, "http://") && xs_strcmp_case(uri->protocol, "https://")
                       && xs_strcmp_case(uri->protocol, "ws://")   && xs_strcmp_case(uri->protocol, "wss://"))) {
        conn->errnum = exs_Error_BadRedirect; 
        return -50;
    }

    //relative path
    if (0) // not sure this is a good idea... new connection seems warranted $$$SREE
    if ((uri->host==0 || xs_strcmp(uri->host, conn->host)==0) &&
        (uri->port==0 || uri->port==conn->port))
        return xs_conn_httprequest (conn, 0, method, uri->path);

    //follow
    err = xs_conn_open (reconn, uri->host ? uri->host : conn->host,
                                uri->port ? uri->port : conn->port, 
                                uri->protocol ? (xs_strcmp_case(uri->protocol, "https://")==0 || xs_strcmp_case(uri->protocol, "wss://")==0) : conn->is_ssl);
    if (err) {conn->errnum = exs_Error_BadRedirect; return -10;}

    //new request
    xs_conn_httprequest (*reconn, 0, method, uri->path);
    uri = xs_uri_destroy(uri); //delete uri first
    return 0;
}


// ==============================================
//  conn read
// ==============================================
size_t xs_conn_read (xs_conn* conn, void* buf, size_t len, int* reread) {
    int n;
    xs_atomic_inc (conn->rcount);
    if (xs_arr_count(conn->wcache) && conn->is_wblocked==0) xs_conn_cachepurge(conn);
    if (conn->ssl)  n = xs_SSL_read (conn->ssl, buf, (int)len);
    else            n = recv (conn->sock, (char*)buf, (int)len, 0);
    if (n<0)        xs_conn_seterr(conn);
    else if (n==0)  conn->errnum = exs_Conn_Close;//-100001;
    else            conn->errnum = 0;
    if (reread)     *reread = (n==len) && (conn->errnum==0);
    if (n>0 && conn->proto==0)  {//check only the first time
        if (conn->ssl) xs_SSL_protocol (conn->ssl, conn->sslproto, sizeof(conn->sslproto));
        conn->proto = 1;
    }
    return n;
}

size_t xs_conn_httpread(xs_conn *conn, void *buf, size_t len, int* reread) {
    const char* h;
    int n, s, nread, err;
    xs_httpreq* req;
    if (conn==0) return 0;
    req = conn->req;
    conn->errnum = 0; //reset error??? $$$SREE
    if (reread) *reread=0;

    //are we done with this payload?
    s = xs_conn_state(conn);
    if (s==exs_Conn_Error ||        //we errored --- reset? --- aborted above, see $$$SREE
        s==exs_Conn_Complete) {     //its done, 
        n = (int)(conn->datalen - conn->consumed);
        if (n>0) memcpy (conn->buf, conn->buf + conn->datalen - n, n);
        conn->datalen = n;
        if (req) req->reqlen = 0;
        conn->consumed = 0;
    } 

    //$$$SREE temporary -- validating assumptions above
    if (req && req->reqlen)
    if (((/*s==exs_Conn_Request || */s==exs_Conn_Redirect) && req->contentlen==0 && req->chunked==0) || //or it was a request/redirect only
        (req && req->statuscode==100 && s==exs_Conn_Response))                                          //or its a response of 100
        {assert(0); xs_logger_error ("sree, you are fart knocker -- see comment in code.");}

    
    //are we looking for a new request?
    if (req==0 || req->reqlen==0 || req->done) {
        n = xs_conn_headerready(conn);
        if (n<=0 || conn->errnum) return n;  //not enough data to read request
        if (req && conn->req==req)  {req->next = conn->freereq; conn->freereq = req; conn->req = 0;  conn->lastreq = 0;} //$$$SREE NOT RIGHT!!!!
        else if (req)               {assert(0);}
        err = xs_http_createreq(conn, n);
        if (err) {conn->errnum = conn->errnum?conn->errnum:err; return -1;}
        req = conn->req; assert(req);
        //conn->datalen=0; if (req) req->reqlen = 0;return n; // for debugging
        if (conn->upgrade) req->upgrade = conn->upgrade; //$$$SREE -- we want to mark it as special -- not sure this is the right semantic

        if (req->upgrade==1) { 
            //this request is done --- its an upgrade request...
            if ((req->method    && (err=xs_websocket_response(conn))<0) ||   //<0 because >0 just means its not a websocket
                (req->method==0 && (err=xs_websocket_accept  (conn))<0)) {
                conn->errnum = exs_Error_WS_InvalidSecKey;
                return -10; 
            }
            if (reread) *reread=(conn->datalen>conn->consumed);
            return 0;  //finished with header
        } else if (req->upgrade==2) {
            if (req->opcode==exs_WS_PING)    xs_conn_write_websocket (conn, exs_WS_PONG, 0, 0, 0);
            if (req->opcode==exs_WS_PONG)    xs_conn_write_websocket (conn, exs_WS_PING, 0, 0, 0);

        } else if (req->contentlen==0 && req->chunked==0) {
            //if its not a reqest and not chunked, and no contentlen, read until socket closes, if there was no content-length
            if (req->method==0 && (h=xs_http_getheader(req, "Content-Length"))==0) {
                req->contentlen = ~(((size_t)1)<<((sizeof(size_t)<<3)-1)); //large length
            } else {
                if (reread) *reread=(conn->datalen>conn->consumed);
                return 0; //this is a zero length request -- no body
            }
        }
    }

    //are we handling chunked?
    if (req->chunked)
    if (req->contentlen == 0 || req->contentlen==req->consumed) {
        //scan for chunk header
        n = xs_conn_chunkready(conn); 
        if (n<=0) {     //don't know next length yet
            if (n==0) { //or we're done with this chunk
                req->chunked = 0;
                if (reread) *reread=1;
            }
            return 0;
        } 
   }
    
    //read the data
    nread = -1;
    if (req->consumed < req->contentlen) {
        //how much left for this request?
        size_t remaining = req->contentlen - req->consumed;
        if (len>remaining)  len = remaining;
        
        //do we have buffered data to use?
        remaining = conn->datalen - conn->consumed;
        if (remaining > 0) {
            if (remaining>len)  remaining = len;
            memcpy(buf, conn->buf + conn->consumed, remaining);
            conn->consumed += remaining;
            nread = remaining;
        } else
            //otherwise read from socket
            nread = xs_conn_read(conn, (char *)buf, (int) len, reread);
        
        //did we get data?
        if (nread>0) req->consumed += nread;
    } else if (reread) *reread=(conn->datalen>conn->consumed);
    
    //done
    if (*(int*)req->datamask!=0) //mask if required
        for (n=0; n<nread; n++) ((char*)buf)[n]^=req->datamask[n&3];
    return nread;
}

// ==============================================
//  conn write
// ==============================================
char xs_conn_writable (xs_conn* conn){
    int sock = conn ? conn->sock : 0;
    return sock ? ((xs_sock_avail(sock, POLLOUT)&POLLOUT)!=0) : 0;
}
size_t xs_conn_header_done (xs_conn* conn, int hasBody) {
    return xs_conn_write_ (conn, "\r\n", 2, hasBody ? MSG_MORE : 0);
}
void xs_conn_body_done (xs_conn* conn) {
    xs_httpreq* req = xs_conn_getreq(conn);
    if (req) req->done = 1;
}
char xs_conn_writeblocked (xs_conn* conn) {
    return conn ? (xs_arr_count(conn->wcache)!=0) : 0;
}
int xs_conn_cacheset   (xs_conn* conn) {
    if(conn==0) return -1; 
    conn->is_wblocked = 1;
    return 0;
}
int xs_conn_cachepurge (xs_conn* conn) {
    size_t tot=conn?xs_arr_count(conn->wcache):0, wtot;
    if (tot==0) return 0;

    //printf ("--removing %d to %d::%d\n", (int)tot, (int)xs_arr_count(conn->wcache), (int)xs_arr_space(conn->wcache));
    wtot = xs_conn_write_(conn, xs_arr_ptr(void,conn->wcache), tot, MSG_MORE);
    if (wtot==tot) {
        xs_arr_reset(conn->wcache); 
        conn->is_wblocked = 0;
        //printf ("--done cleared %d to %d::%d\n", (int)tot, (int)xs_arr_count(conn->wcache), (int)xs_arr_space(conn->wcache)); 
        return 0;
    }

    xs_arr_remove(char, conn->wcache, 0, wtot);
    //printf ("--done removing %d to %d::%d\n", (int)tot, (int)xs_arr_count(conn->wcache), (int)xs_arr_space(conn->wcache)); 
    return -1;
}
int xs_conn_cachefill (xs_conn* conn, const void* buf, size_t len, int force) {
    const size_t maxcache=1000000;
    return 0;
    if ((force || xs_arr_count(conn->wcache) || conn->is_wblocked) && xs_arr_ptrinrange(char,conn->wcache,buf)==0) {
        if (force || (conn->is_wblocked && len+xs_arr_count(conn->wcache)<=maxcache) || //don't check writable if we're blocked (unless we are at limit) 
            xs_conn_writable(conn)==0 || xs_conn_cachepurge(conn)!=0) {
            //printf ("--adding %d to %d::%d\n", (int)len, (int)xs_arr_count(conn->wcache), (int)xs_arr_space(conn->wcache));
            if (len+xs_arr_count(conn->wcache)>maxcache || 
                xs_arr_add(char,conn->wcache,(char*)buf, (int)(len))==0) {
                conn->errnum=-108;
                return 0;
            }
           //printf ("--done adding %d to %d::%d\n", (int)len, (int)xs_arr_count(conn->wcache), (int)xs_arr_space(conn->wcache));
           if (conn->errnum==0) conn->errnum = exs_Error_WriteBusy;
            return -1;
        }
    }
    return 0;
}


size_t xs_conn_write (xs_conn* conn, const void* buf, size_t len) {return xs_conn_write_(conn, buf, len, 0);}
size_t xs_conn_write_header (xs_conn* conn, const void* buf, size_t len) {return xs_conn_write_(conn, buf, len, MSG_MORE);}
size_t xs_conn_write_ (xs_conn* conn, const void* buf, size_t len, int flags) {
#if 0//defined(WIN32) && defined(_WS2DEF_) //no performance benefit?  worth re-testing... incorrect now - no write cache stuff
    WSABUF DataBuf;
    DWORD SendBytes;
    size_t tot=0;
    int n, amt;
    while (tot<len) {
        amt = (len-tot)>INT_MAX ? INT_MAX : (int)(len-tot);
        if (conn->ssl) n = xs_SSL_write (conn->ssl, (char*)buf + tot, amt);
        else {
            DataBuf.buf = (char*)buf + tot;
            DataBuf.len = amt;
            n = WSASend (conn->sock, &DataBuf, 1, &SendBytes, flags, 0, NULL);
        }
        if (n<0) return tot ? tot : n;
        tot += SendBytes;
    }
#else
    size_t tot=0;
    int n, amt;

    conn->errnum = 0;
    if (xs_conn_cachefill(conn, buf, len, 0)) return len;

    if ( (flags&MSG_MORE) && conn->corked==0) xs_sock_settcpcork (conn->sock, (conn->corked=1));
    while (tot<len) {
        amt = (len-tot)>INT_MAX ? INT_MAX : (int)(len-tot);
        if (conn->ssl)  n = xs_SSL_write (conn->ssl, (char*)buf + tot, amt);
        else if (1)     n = send (conn->sock, (char*)buf + tot, amt, 0); //flags handled by TCP_CORK
        else            n = send (conn->sock, (char*)buf + tot, amt, flags);
        //if (n<=0)    return tot;
        if (n==0)      return 0; //$$SREE normal socket termination
        tot += (n>0 ? n : 0);
        if (n!=amt) {
            if (xs_conn_cachefill(conn, (char*)buf + tot, amt, 1)) tot += amt;
            if (n>0 || xs_conn_seterr(conn)==0) conn->errnum = exs_Error_WriteBusy;
            break;
        }
    }
    if (!(flags&MSG_MORE) && conn->corked==1) xs_sock_settcpcork (conn->sock, (conn->corked=0));
#endif
    return tot;
}

size_t xs_conn_write_chunked(xs_conn *conn, const void *buf, size_t len) {
    int rret;
    if (buf==0 || len==0) return xs_conn_printf_header(conn, "0\r\n\r\n");

    rret  = xs_conn_printf_header(conn, "%x\r\n", (int)len);
    rret += xs_conn_write(conn, buf, len);
    rret += xs_conn_printf_header(conn, "\r\n");
    return rret;
}

int xs_conn_print_cb(void* userdata, const char* s, int len) {/*size_t n=*/xs_conn_write((xs_conn*)userdata, s, len); return len;}
int xs_conn_print_more_cb(void* userdata, const char* s, int len) {/*size_t n=*/xs_conn_write_((xs_conn*)userdata, s, len, MSG_MORE); return len;}
int xs_conn_printf(xs_conn *conn, const char *fmt, ...) {
    va_list ap;
    int result;
    if (fmt==0 || *fmt==0) return 0;
    va_start(ap, fmt);
    result = xs_sprintf_core (xs_conn_print_cb, conn, 0, 0, fmt, ap);
    va_end(ap);
    return result;
}
int xs_conn_printf_va(xs_conn *conn, const char *fmt, va_list apin) {
    va_list ap;
    int result;
    if (fmt==0 || *fmt==0) return 0;
    va_copy(ap, apin);
    result = xs_sprintf_core (xs_conn_print_cb, conn, 0, 0, fmt, ap);
    va_end(ap);
    return result;
}
int xs_conn_print_header_cb(void* userdata, const char* s, int len) {return xs_conn_write_header((xs_conn*)userdata, s, len);}
int xs_conn_printf_header(xs_conn *conn, const char *fmt, ...) {
    va_list ap;
    int result;
    if (fmt==0 || *fmt==0) return 0;
    va_start(ap, fmt);
    result = xs_sprintf_core (xs_conn_print_header_cb, conn, 0, 0, fmt, ap);
    va_end(ap);
    return result;
}
int xs_conn_printf_header_va(xs_conn *conn, const char *fmt, va_list apin) {
    va_list ap;
    int result;
    if (fmt==0 || *fmt==0) return 0;
    va_copy(ap, apin);
    result = xs_sprintf_core (xs_conn_print_header_cb, conn, 0, 0, fmt, ap);
    va_end(ap);
    return result;
}
int xs_conn_printf_chunked(xs_conn *conn, const char *fmt, ...) {
    va_list ap, ap_copy;
    int len;
    
    //terminate chunk
    if (fmt==0 || *fmt==0) return xs_conn_write_chunked(conn, 0, 0);
    
    //otherwise print it
    va_start(ap, fmt);
    va_copy(ap_copy, ap);
    len  = xs_sprintf_core (0, 0, 0, 0, fmt, ap_copy);
    va_end(ap_copy);
    
    len  = xs_conn_printf_header(conn, "%x\r\n", (int)len);
    va_copy(ap_copy, ap);
    len += xs_sprintf_core (xs_conn_print_more_cb, conn, 0, 0, fmt, ap_copy);
    va_end(ap_copy);
    len += xs_conn_printf_header(conn, "\r\n");
    va_end(ap);
    
    return len;
}


// =================================================================================================================
//  async implementation
// =================================================================================================================
struct xs_async_connect {
    pthread_t           th;
    int                 stop;
    xs_array            conn_arr;
    struct xs_pollfd*   xp;
    void*               userdata;
};

int xs_async_handler(struct xs_pollfd* xp, int message, int sockfd, int xptoken, void* userdata) {
    struct xs_async_connect* xas = (struct xs_async_connect*)xs_pollfd_get_userdata(xp);
    xs_conn* conn = (xs_conn*)userdata;
    xs_async_callback* cb = (xs_async_callback*)(conn ? conn->cb : 0);
    //char str[256];
    int err = 0, port;
    if (xas==0) return -1;
    if (xas->stop) 
        xs_pollfd_stop(xp);

    switch (message) {
        case exs_pollfd_New:
            //current userData is from the listening socket -- not current one
            if (conn->sock==sockfd) {
                xs_sock_setnonblocking (sockfd, 1);
            } else {
                port = conn->port;
                err = xs_conn_accept (&conn, sockfd, xs_pollfd_getsocket_addr(xp,xptoken), conn->sslctx);
                if (err) break;
                conn->port = port; //preserve port we listened on -- but what about host? $$$SREE
                conn->cb = cb;
                //xs_printf ("accept %s\n", xs_sock_addrtostr(str, sizeof(str), &conn->sa)); //debugging
            }
            xs_pollfd_setsocket_userdata(xp, xptoken, conn);
            if (cb&&xas->stop==0) err = (*cb) (xas, exs_Conn_New, conn);
        break;

        case exs_pollfd_Idle:
            if (cb&&xas->stop==0) err = (*cb) (xas, exs_Conn_Idle, conn);
        break;

        case exs_pollfd_Read:
          if (cb&&xas->stop==0) err = (*cb) (xas, exs_Conn_Read, conn);
        break;

        case exs_pollfd_Write:
            if (xs_conn_cachepurge(conn)==0) {
                if (cb&&xas->stop==0) err = (*cb) (xas, exs_Conn_Write, conn);
                if (err!=exs_Conn_Write) xs_pollfd_setsocket_events (xp, xptoken, POLLIN);
                }
        break;

        case exs_pollfd_Error:
        case exs_pollfd_Delete:
            xs_pollfd_clear (xp, xptoken);          
            if (cb&&xas->stop==0) err = (*cb) (xas, message==exs_pollfd_Error ? exs_Conn_Error : exs_Conn_Close, conn);
            if (err!=exs_Conn_Close) {err=exs_Conn_Close; conn=xs_conn_dec(conn);}
        break;
    }

    //write failure...
    if (err!=exs_Conn_Close) //it got closed -- don't check it
    if (conn && (err==0 || conn->errnum==exs_Error_WriteBusy) && 
        xs_conn_writeblocked(conn))
        err = exs_Conn_Write;

    //catching commands from callback
    switch (err) {
        case exs_Conn_Write:
            conn->is_wblocked = 1;
            xs_pollfd_setsocket_events (xp, xptoken, POLLIN | POLLOUT);
            break;
        case exs_Conn_Close:
            xs_pollfd_clear (xp, xptoken);
            break;
    }
    return err;
}

void * async_threadproc(struct xs_async_connect* xas) {
    xs_pollfd_run(xas->xp);
    //xs_pollfd_dec(xas->xp);
    return 0;
}

struct xs_async_connect*  xs_async_create(int hintsize) {
    struct xs_async_connect* xas = (struct xs_async_connect*)calloc (sizeof(struct xs_async_connect), 1);
    if (xas==0)                 return 0;
    xas->xp                     = xs_pollfd_create (xs_async_handler, 0, hintsize);
    xs_pollfd_inc               (xas->xp);
    if (xas->xp==0)             {free(xas); return 0;}
    xs_pollfd_set_userdata      (xas->xp, xas);
    pthread_create_detached     (&xas->th, (xs_thread_proc)async_threadproc, xas);
    if (xas->th==0)             {xs_pollfd_destroy(xas->xp); free(xas); return 0;}
    return xas;
}

int xs_async_active(struct xs_async_connect* xas)                       {return xas&&xas->stop==0;}
int xs_async_print(struct xs_async_connect* xas)                        {return xs_pollfd_print (xas->xp);}
void xs_async_setuserdata(struct xs_async_connect* xas, void* usd)      {if (xas) xas->userdata=usd;}
void* xs_async_getuserdata(struct xs_async_connect* xas)                {return xas ? xas->userdata : 0;}
void xs_async_stop(struct xs_async_connect* xas)                        {if (xas) xas->stop=-2;}

struct xs_async_connect* xs_async_destroy(struct xs_async_connect* xas) {
    int i;
    if (xas==0) return 0;
    xas->stop = 1;
    xs_pollfd_stop(xas->xp);
    xs_pollfd_dec(xas->xp);
    for (i=0; i<xs_arr_count(xas->conn_arr); i++)
        xs_arr(xs_conn*,xas->conn_arr, i) = xs_conn_dec (xs_arr_ptr(xs_conn*,xas->conn_arr)[i]);
    xs_arr_destroy(xas->conn_arr);
    free(xas);
    return 0;
}

struct xs_async_connect*  xs_async_read(struct xs_async_connect* xas, xs_conn* conn, xs_async_callback* proc) {
    if (conn==0) return 0;
    if (xas==0) xas = xs_async_create(0);
    if (xas==0) return 0;
    conn->cb = proc;
    xs_conn_inc(conn);
    xs_pollfd_push (xas->xp, xs_conn_getsock(conn), conn, 0); 
    xs_arr_add (xs_conn*, xas->conn_arr, &conn, 1);
    return xas;
}

struct xs_async_connect*  xs_async_listen(struct xs_async_connect* xas, xs_conn* conn, xs_async_callback* proc) {
    if (conn==0) return xas;
    if (xas==0) xas = xs_async_create(0);
    if (xas==0) return 0;
    conn->cb = proc;
    xs_conn_inc(conn);
    xs_pollfd_push (xas->xp, xs_conn_getsock(conn), conn, 1); 
    xs_arr_add (xs_conn*, xas->conn_arr, &conn, 1);
    return xas;
}

int xs_async_call (xs_async_connect* xas, int message, xs_conn* conn){
    return xas&&conn ? (*conn->cb) (xas, message, conn) : 0;
}

int xs_async_defaulthandler (struct xs_async_connect* xas, int message, xs_conn* conn) {
    xs_server_ctx* ctx = (xs_server_ctx*)xs_async_getuserdata (xas);
    const xs_httpreq* req;
    const char* h;
    char buf[1024];
    int n, s, sp, rr=1, err=0;

    switch (message) {
        case exs_Conn_New:
            break;

        case exs_Conn_Read:
            //web socket handling
            if (conn->upgrade==2) {
                err = xs_async_call (xas, exs_Conn_WSRead, conn);   
                if (err) return err;
            }

            //normal http connections
            sp = xs_conn_state (conn);
            while (err==0 && rr) {
                n = xs_conn_httpread(conn, buf, sizeof(buf)-1, &rr);
                s = xs_conn_state (conn);
                if (err) return err;

                //====================
                // give callback a shot
                //====================
                switch (s) {
                    case exs_Conn_Response:
                        if (sp!=exs_Conn_Response) err = xs_async_call (xas, exs_Conn_HTTPNew, conn);
                        if (err) return err;
                        err = xs_async_call (xas, exs_Conn_HTTPRead, conn);   
                        s = xs_conn_state (conn);
                        if (s!=exs_Conn_Complete) break;
                        sp = exs_Conn_Response; //don't trigger exs_Conn_HTTPNew below
                        //fall through

                    case exs_Conn_Complete:
                        if (sp!=exs_Conn_Response) err = xs_async_call (xas, exs_Conn_HTTPNew, conn);
                        if (err) return err;
                        err = xs_async_call (xas, exs_Conn_HTTPComplete, conn);   
                        break;
                }
                if (err) return err;
                if (s==exs_Conn_Error) {
                    //====================
                    // error case
                    //====================
                    err = xs_conn_error(conn);
                    switch (err) {
                        case exs_Error_HeaderTooLarge:  xs_conn_write_httperror (conn, 414, "Header Too Large", "Max Header Sizer Exceeded");   break;
                        case exs_Error_InvalidRequest:  xs_conn_write_httperror (conn, 501, "Not Supported", "Invalid Request");                break;
                    }
                } else if (s==exs_Conn_Complete) {
                    req = xs_conn_getreq(conn);
                    h   = req&&req->upgrade ? 0 : xs_http_get(req, exs_Req_Method);
                    //====================
                    // request
                    //====================
                    if (h && (!strcmp(h, "GET") || !strcmp(h, "HEAD"))) {
                        if (1) {
                            size_t result;
                            char path[PATH_MAX]="";
                            n = xs_strappend (path, sizeof(path), ctx->document_root);
                            xs_strlcat (path+n, xs_http_get (req, exs_Req_URI), sizeof(path));

                            //xs_conn_write_header (conn, msghdr, sizeof(msghdr)-1);
                            //xs_conn_write (conn, msg, sizeof(msg)-1);
                            result = xs_http_fileresponse (ctx, conn, path, h? *h=='G' : 1); //GET vs HEAD
                            if (1)
                                xs_logger_debug ("%s - - \"%s %s HTTP/%s\" %d %zd", 
                                            xs_conn_getsockaddrstr (conn),
                                            xs_http_get (req, exs_Req_Method), 
                                            xs_http_get (req, exs_Req_URI),
                                            xs_http_get (req, exs_Req_Version),
                                            xs_http_getint (req, exs_Req_Status), result);
                        }
                    } else if (h) xs_logger_error ("HTTP method '%s' not handled %s", h ? h : "UNSPECIFIED", xs_conn_getsockaddrstr (conn));
                    if (xs_http_getint(req, exs_Req_KeepAlive)==0) {
                        err = exs_Conn_Close;
                    }
                }
            }
            if (err==0) break;

            //fallthrough...
            if (err && err!=exs_Conn_Close) xs_logger_error ("closing connections from %s %d", xs_conn_getsockaddrstr (conn), err);

        case exs_Conn_Error:
        case exs_Conn_Close:
            xs_conn_dec(conn);
            err = exs_Conn_Close;
            break;
    }

   return err;
}


// =================================================================================================================
//  uri implementation
// =================================================================================================================
xs_uri* xs_uri_create(const char* instr, int forceHost) {
    xs_uri* uri;
    const char* str=instr;
    char* buf, *be, *p=0;
    int c, len;
    if (str==0||(len=strlen(str))==0) return 0;
    len += 5 + 8 + 2; //terminating 0s for protocol,host,path,query,hash + "file://" + "/" (default path)
    uri = (xs_uri*)calloc(1, sizeof(xs_uri)+len);
    buf = (char*)(uri+1);
    be  = buf+len;

    //find protocol
    c=0;
    while (str[c]!=0) {
        if (str[c]==':') {
            if (c>1) {
                //web protocol
                uri->protocol = buf; memcpy (buf, str, c);
                buf[c]=0; buf+=c+1; str+=c+1; //str+1 to skip ':'
                if      (!xs_strcmp(uri->protocol, "http"))     uri->port=80;
                else if (!xs_strcmp(uri->protocol, "https"))    uri->port=443;
                else if (!xs_strcmp(uri->protocol, "ws"))       uri->port=80;
                else if (!xs_strcmp(uri->protocol, "wss"))      uri->port=443;
                else if (!xs_strcmp(uri->protocol, "ftp"))      uri->port=21;
            } else if (c>0) {
                //drive letter -- as in google chrome
                uri->protocol = buf;
                xs_strlcpy(buf, "file://", len);
                buf+=strlen(buf)+1;
            }
            break;
        } else if (str[c]=='/' || str[c]=='\\' || str[c]=='[') {
            //no protocol
            break;
        }
        c++;
    }

    //special convenience reset case -- only known protocols are OK 
    //e.g. to parse 'localhost:8080/someURL' and not interpret 'localhost' as a procotol
    if (forceHost==2 && uri->port==0) {uri->protocol = 0; buf = (char*)(uri+1); str=instr;} 

    //find hostname
    c=0;  
    len=(int)(be-buf);
    p = (str[0]=='[') ? strchr((char*)str, ']') : 0; //IPv6
    if (uri->protocol==0 || xs_strcmp_case(uri->protocol, "file://"))   //not a file path
    if (forceHost || (str[0]=='/' && str[1]=='/') ||                    //per RFC 3986
        (p!=0) ||                                                       //IPv6 address
        (uri->protocol && (!xs_strcmp_case(uri->protocol, "http")  ||   //as with google chrome
                           !xs_strcmp_case(uri->protocol, "https") ||
                           !xs_strcmp_case(uri->protocol, "ftp")   ||
                           !xs_strcmp_case(uri->protocol, "ftps")  ||
                           !xs_strcmp_case(uri->protocol, "ws")    ||
                           !xs_strcmp_case(uri->protocol, "wss"))) ||
        (uri->protocol && strchr(str, '/')==0 && strchr(str, '\\')==0)) { //per RFC 3986
        if (p) {
            //IPv6
            str++;
            uri->host = buf; memcpy (buf, str, (int)(p-str));
            buf += (int)(p-str);
            *buf++ = 0;
            str  = p+1;

            //get port
            if (str[0]==':') p=(char*)str; else p=0;
            while (str[c] && str[c]!='/' && str[c]!='\\') c++;
            if (p) {memcpy (buf, str+1, c-1); buf[c-1]=0; uri->port=atoi(buf);}
            str += c;
        } else {
            while (str[c]!=0 && (str[c]=='/' || str[c]=='\\')) c++;
            str+=c;
            c=0;
            while (str[c] && str[c]!='/' && str[c]!='\\') c++;
            if (c>0) {
                uri->host = buf; memcpy (buf, str, c);
                buf[c]=0; buf+=c+1; str+=c;

                //get port
                p=strchr(uri->host, ':');
                if (p) {buf=p; *buf++=0; uri->port = atoi(buf);}
            }
        }
    }

    //find path
    c=0; 
    len=(int)(be-buf);
    while (str[c] && str[c]!='?' && str[c]!='#') c++;
    if (c>0) {
        uri->path = buf; 
        if (str[0]!='/' && xs_strcmp_case(uri->protocol, "file://")!=0) *buf++='/';
        memcpy (buf, str, c);
        buf[c]=0; buf+=c+1; str+=c;
    } else if (xs_strcmp_case(uri->protocol, "file://")!=0) {
        //create "root" path
        uri->path = buf;
        xs_strlcpy (buf, "/", len);
        buf+=2;//char + termination
    }

    //find query
    if (str[0]=='?') {
        str++;
        c=0; 
        len=(int)(be-buf);
        while (str[c] && str[c]!='#') c++;
        if (c>0) {
            uri->query = buf;  memcpy (buf, str, c);
            buf[c]=0; buf+=c+1; str+=c;
        }
    }

    //find hash
    if (str[0]=='#') {
        str++;
        len=strlen(str); 
        c=len;
        if (c>0) {
            uri->hash = buf; memcpy (buf, str, c);
            buf[c]=0; buf+=c+1; str+=c;
        }
    }

    //success
    return uri;
}

xs_uri*  xs_uri_destroy(xs_uri* uri) {
    if (uri) free(uri);
    return 0;
}

int xs_uri_decode(char* dst, int dlen, const char* src, int slen, char formencoded) { //src and dst may be the same
    int s=0,d=0,err=0,r;
    dlen--;//for terminating 0
    if (dlen<=0) return -50;
    if (formencoded) formencoded='+';
    while (s<slen && d<dlen && src[s]) {
        if (src[s]==formencoded)            {dst[d++] = ' '; s++;}
        else if (src[s]!='%')               {dst[d++] = src[s++];}
        else if (s+2<slen && d+1<dlen)      {if (xs_fromhex(src[s+1])<0 || xs_fromhex(src[s+2])<0) err=-1;
                                             else dst[d++] = (xs_fromhex(src[s+1])<<4) + xs_fromhex(src[s+2]); s+=3;}
        else                                {err=-1; break;}
    }
    if (err==0) {r=slen-s; if(r && d+r<=dlen) {memcpy(dst+d,src+s,r);d+=r;}}
    dst[d] = 0;
    return err;
}

int xs_uri_encode(char* dst, int dlen, const char* src, int slen, char formencoded) { //src and dst may NOT be the same
    int s=0,d=0,err=0,c;
    dlen--;//for terminating 0
    if (dlen<=0 || src==dst) return -50;
    if (formencoded) formencoded=' ';
    while (s<slen && d<dlen && src[s]) {
        if (src[s]==formencoded)            {dst[d++] = '+'; s++;}
        else if (xs_http_okchar(src[s]) && src[s]!='%')     
            dst[d++] = src[s++];
        else if (d+2<dlen) {
            c = (int)((unsigned int)src[s++]);
            dst[d++] = '%';
            dst[d++] = xs_tohex(c>>4, 1);
            dst[d++] = xs_tohex(c&15, 1);
        }
    }
    dst[d] = 0;
    return err;
}

#endif //_xs_CONNECT_IMPL_
#endif //_xs_IMPLEMENTATION_
