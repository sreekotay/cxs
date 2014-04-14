// =================================================================================================================
// xs_pipe.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_PIPE_H_
#define _xs_PIPE_H_


// =================================================================================================================
// function declarations
// =================================================================================================================
typedef struct xs_pipe {
    int r, w;               //read and write pipes
} xs_pipe;

int xs_pipe_open    (xs_pipe* p);
int xs_pipe_close   (xs_pipe* p);
int xs_pipe_write   (xs_pipe* p, char* buf, int len);
int xs_pipe_read    (xs_pipe* p, char* buf, int len);

#endif //header





// =================================================================================================================
//  implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_PIPE_IMPL_
#define _xs_PIPE_IMPL_



int xs_pipe_open(xs_pipe* p) {
#ifdef WIN32
    /*
    struct sockaddr_in sa = {0};
    int len = sizeof(sa);

    memset (p, 0, sizeof(*p));
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return -1;

    sa.sin_family = AF_INET;
    sa.sin_port = htons(0);
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(sock, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR ||
        listen(sock, SOMAXCONN) ||
        getsockname(sock, (SOCKADDR*)&sa, &len)||
        (p->w=socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        closesocket(sock); 
        return -1;
    }
    if (connect(p->w, (struct sockaddr*)&sa, len) ||
       (p->r=accept(sock, (struct sockaddr*)&sa, &len)) == INVALID_SOCKET) {
        closesocket((SOCKET)p->w);
        p->w = 0;
        closesocket(sock);
        return -1;
    }
    closesocket(sock);
    return 0;
    */
    return _pipe((int*)p, 8192, _O_BINARY)
#else
    return pipe((int*)p);
#endif
}

int xs_pipe_read(xs_pipe* p, char* buf, int len) {
#ifdef WIN32
    int ret = recv(p->r, buf, len, 0);
    if (ret<0 && WSAGetLastError()==WSAECONNRESET) return 0;
    return ret;
#else
    return read(p->r, buf, len);
#endif
}

int xs_pipe_write(xs_pipe* p, char* buf, int len) {
#ifdef WIN32
    return send(p->w, buf, len, 0);
#else
    return write(p->w, buf, len);
#endif
}

int xs_pipe_close(xs_pipe* p) {
#ifdef WIN32
    if (p->w>0) closesocket(p->w);
    if (p->r>0) closesocket(p->r);
#else
    if (p->w>0) close(p->w);
    if (p->r>0) close(p->r);
#endif
    memset(p, 0, sizeof(*p));
    return 0;
}

#endif //_xs_PIPE_IMPL_
#endif //_xs_IMPLEMENTATION_