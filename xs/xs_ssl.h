// =================================================================================================================
// xs_ssl.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_SSL_H_
#define _xs_SSL_H_

#include "xs_posix_emu.h"
#include "xs_atomic.h"

// =================================================================================================================
//  SSL functions -- declarations
// =================================================================================================================
struct SSL_st;
struct SSL_ctx_st;
typedef struct SSL_st       SSL;
typedef struct SSL_ctx_st   SSL_CTX;

//init
char            xs_SSL_ready            ();
int             xs_SSL_initialize       ();
void            xs_SSL_uninitialize     ();
const char*     xs_SSL_error            (SSL* ssl);

//context
SSL_CTX*        xs_SSL_newCTX_server    ();
int             xs_SSL_set_certs        (SSL_CTX* s, char* privateKeyPem, char* certPem, char* certChainPem); //needs to be set for server CTX to work
SSL_CTX*        xs_SSL_newCTX_client    ();
void            xs_SSL_freeCTX          (SSL_CTX* sslctx);

//connection
char            xs_sslize_accept        (SSL **ssl, SSL_CTX *s, int sock);
char            xs_sslize_connect       (SSL **ssl, SSL_CTX *s, int sock);
char            xs_SSL_protocol         (SSL* ssl, char* str, int maxlen);//call after accept or connect
int             xs_SSL_read             (SSL *ssl, void *buff, int len);
int             xs_SSL_write            (SSL *ssl, const void *buff, int len);
char            xs_SSL_free             (SSL **ssl);

#endif //_xs_SSL_H_







// =================================================================================================================
//  implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_SSL_IMPL_
#define _xs_SSL_IMPL_

// Portions Copyright (c) 2007-2014 Sree Kotay
//  modified to use xs_namespace and re-factored into separate header

// Copyright (c) 2004-2013 Sergey Lyubka
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// SSL loaded dynamically from DLL.
// I put the prototypes here to be independent from Openxs_SSL source installation.

#ifdef _xs_NO_SSL_DL_
#include <openssl/ssl.h>
#include <openssl/err.h>
#else //_xs_NO_SSL_DL_
typedef struct SSL_method_st                    SSL_METHOD;

#define SSL_CTX_set_options(ctx,op)             SSL_CTX_ctrl((ctx),SSL_CTRL_OPTIONS,(op),NULL)
#define SSL_CTX_set_mode(ctx,op)                SSL_CTX_ctrl((ctx),SSL_CTRL_MODE,(op),NULL)
#define SSL_CTRL_OPTIONS                        32
#define SSL_CTRL_MODE                           33
#define SSL_OP_NO_SSLv2                         0x01000000L
#define SSL_OP_ALL                              0x80000BFFL
#define SSL_OP_NO_COMPRESSION                   0x00020000L
#define SSL_MODE_ENABLE_PARTIAL_WRITE           (0x1)
#define SSL_MODE_AUTO_RETRY                     (0x4)
#define SSL_MODE_RELEASE_BUFFERS                0x00000010L 

struct xs_SSL_func {
    const char *name;   // SSL function name
    void  (*ptr)(void); // Function pointer
};

#define SSL_free                                (* (void (*)(SSL *)) xs_SSL_sw[0].ptr)
#define SSL_accept                              (* (int (*)(SSL *)) xs_SSL_sw[1].ptr)
#define SSL_connect                             (* (int (*)(SSL *)) xs_SSL_sw[2].ptr)
#define SSL_read                                (* (int (*)(SSL *, void *, int)) xs_SSL_sw[3].ptr)
#define SSL_write                               (* (int (*)(SSL *, const void *,int)) xs_SSL_sw[4].ptr)
#define SSL_get_error                           (* (int (*)(SSL *, int)) xs_SSL_sw[5].ptr)
#define SSL_set_fd                              (* (int (*)(SSL *, int)) xs_SSL_sw[6].ptr)
#define SSL_new                                 (* (SSL * (*)(SSL_CTX *)) xs_SSL_sw[7].ptr)
#define SSL_CTX_new                             (* (SSL_CTX * (*)(SSL_METHOD *)) xs_SSL_sw[8].ptr)
#define SSLv23_server_method                    (* (SSL_METHOD * (*)(void)) xs_SSL_sw[9].ptr)
#define SSL_library_init                        (* (int (*)(void)) xs_SSL_sw[10].ptr)
#define SSL_CTX_use_PrivateKey_file             (* (int (*)(SSL_CTX *, const char *, int)) xs_SSL_sw[11].ptr)
#define SSL_CTX_use_certificate_file            (* (int (*)(SSL_CTX *, const char *, int)) xs_SSL_sw[12].ptr)
#define SSL_CTX_set_default_passwd_cb           (* (void (*)(SSL_CTX *, mg_callback_t)) xs_SSL_sw[13].ptr)
#define SSL_CTX_free                            (* (void (*)(SSL_CTX *)) xs_SSL_sw[14].ptr)
#define SSL_load_error_strings                  (* (void (*)(void)) xs_SSL_sw[15].ptr)
#define SSL_CTX_use_certificate_chain_file      (* (int (*)(SSL_CTX *, const char *)) xs_SSL_sw[16].ptr)
#define SSLv23_client_method                    (* (SSL_METHOD * (*)(void)) xs_SSL_sw[17].ptr)
#define SSL_pending                             (* (int (*)(SSL *)) xs_SSL_sw[18].ptr)
#define SSL_CTX_set_verify                      (* (void (*)(SSL_CTX *, int, int)) xs_SSL_sw[19].ptr)
#define SSL_CTX_set_next_protos_advertised_cb   (* (void (*)(SSL_CTX *s, int (*cb) (SSL *ssl, const unsigned char **out, unsigned int *outlen, void *arg), void *arg)) xs_SSL_sw[20].ptr)
#define SSL_get0_next_proto_negotiated          (* (void (*)(SSL *ssl, const unsigned char **data, unsigned *len)) xs_SSL_sw[21].ptr)
#define SSL_CTX_set_alpn_select_cb              (* (void (*)(SSL* ssl, const unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen, void *arg)) xs_SSL_sw[22].ptr)
#define SSL_CTX_ctrl                            (* (int (*)(SSL_CTX *, int cmd,long larg,void *parg)) xs_SSL_sw[23].ptr)
#define SSL_CTX_set_session_id_context          (* (int (*)(SSL_CTX *ctx, const unsigned char *sid_ctx, unsigned int sid_ctx_len)) xs_SSL_sw[24].ptr)
#define SSL_set_session_id_context              (* (int (*)(SSL *ssl, const unsigned char *sid_ctx, unsigned int sid_ctx_len)) xs_SSL_sw[25].ptr)
#define SSL_shutdown                            (* (int (*)(SSL *)) xs_SSL_sw[26].ptr)

#define CRYPTO_num_locks                        (* (int (*)(void)) xs_crypto_sw[0].ptr)
#define CRYPTO_set_locking_callback             (* (void (*)(void (*)(int, int, const char *, int))) xs_crypto_sw[1].ptr)
#define CRYPTO_set_id_callback                  (* (void (*)(unsigned long (*)(void))) xs_crypto_sw[2].ptr)
#define ERR_get_error                           (* (unsigned long (*)(void)) xs_crypto_sw[3].ptr)
#define ERR_error_string                        (* (char * (*)(unsigned long,char *)) xs_crypto_sw[4].ptr)


// set_xs_SSL_option() function updates this array.
// It loads SSL library dynamically and changes NULLs to the actual addresses
// of respective functions. The macros above (like xs_SSL_connect()) are really
// just calling these functions indirectly via the pointer.
static struct xs_SSL_func xs_SSL_sw[] = {
  {"SSL_free",   NULL},
  {"SSL_accept",   NULL},
  {"SSL_connect",   NULL},
  {"SSL_read",   NULL},
  {"SSL_write",   NULL},
  {"SSL_get_error",  NULL},
  {"SSL_set_fd",   NULL},
  {"SSL_new",   NULL},
  {"SSL_CTX_new",   NULL},
  {"SSLv23_server_method", NULL},
  {"SSL_library_init",  NULL},
  {"SSL_CTX_use_PrivateKey_file", NULL},
  {"SSL_CTX_use_certificate_file",NULL},
  {"SSL_CTX_set_default_passwd_cb",NULL},
  {"SSL_CTX_free",  NULL},
  {"SSL_load_error_strings", NULL},
  {"SSL_CTX_use_certificate_chain_file", NULL},
  {"SSLv23_client_method", NULL},
  {"SSL_pending", NULL},
  {"SSL_CTX_set_verify", NULL},
  {"SSL_CTX_set_next_protos_advertised_cb", NULL},
  {"SSL_get0_next_proto_negotiated", NULL},
  {"SSL_CTX_set_alpn_select_cb", NULL},
  {"SSL_CTX_ctrl", NULL},
  {"SSL_CTX_set_session_id_context", NULL},
  {"SSL_set_session_id_context", NULL},
  {"SSL_shutdown",   NULL},
  {NULL,    NULL}
};

// Similar array as xs_SSL_sw. These functions could be located in different lib.
static struct xs_SSL_func xs_crypto_sw[] = {
  {"CRYPTO_num_locks",  NULL},
  {"CRYPTO_set_locking_callback", NULL},
  {"CRYPTO_set_id_callback", NULL},
  {"ERR_get_error",  NULL},
  {"ERR_error_string", NULL},
  {NULL,    NULL}
};


#if defined(SSL_LIB)
#define xs_SSL_LIB                              SSL_LIB
#elif defined WIN32
#define xs_SSL_LIB                              "ssleay32.dll"
#elif defined __MACH__
#define xs_SSL_LIB                              "libssl.dylib"
#else
#define xs_SSL_LIB                              "libssl.so"
#endif

#if defined(CRYPTO_LIB)
#define xs_CRYPTO_LIB                           CRYPTO_LIB
#elif defined WIN32
#define xs_CRYPTO_LIB                           "libeay32.dll"
#elif defined __MACH__
#define xs_CRYPTO_LIB                           "libcrypto.dylib"
#else
#define xs_CRYPTO_LIB                           "libcrypto.so"
#endif //WIN32


// =================================================================================================================
// dynamic DLL loader
// =================================================================================================================
static int xs_LoadDLL_Funcs(void *dll_handle,  struct xs_SSL_func *sw) {
    union {void *p; void (*fp)(void);} u;
    struct xs_SSL_func *fp;
    int ret=0;
    
    for (fp=sw; fp->name!=NULL; fp++) {
        u.p = dlsym(dll_handle, fp->name);
        if (u.fp == NULL)   ret=-2;
        else                {fp->ptr = u.fp; ret=0;}
    }
    
    return ret;
}


#endif //_xs_NO_SSL_DL_



// =================================================================================================================
//  SSL functions -- implementation
// =================================================================================================================
xs_atomic xs_SSL_refcount=0;
pthread_mutex_t *xs_SSL_mutexes=0;
void *xs_SSL_lib = 0;
void *xs_crypto_lib = 0;

char    xs_SSL_ready ()                                         {return xs_SSL_refcount!=0;}
int     xs_SSL_read  (SSL *ssl, void *buff, int len)            {return SSL_read(ssl,buff,len);}
int     xs_SSL_write (SSL *ssl, const void *buff, int len)      {return SSL_write(ssl,buff,len);}

char xs_SSL_protocol(SSL* ssl, char* data, int maxlen) {
    /*
    const unsigned char*s; unsigned sl;
    if (data) *data=0; else return 0;
    if (SSL_get0_next_proto_negotiated==0) return 0;
    SSL_get0_next_proto_negotiated (ssl, &s, &sl);
    if (s==0 || *s==0 || sl==0) return 0;
    if (sl>(unsigned)maxlen-1) sl=maxlen-1;
    memcpy(data, s, sl);
    data[sl]=0;
    */
    return 1;
}
char xs_sslize_accept(SSL **ssl, SSL_CTX *s, int sock) {
  int c;
  if (ssl==0) return 0;
  
  if ((*ssl)==0) {
      (*ssl) = SSL_new(s); 
      if ((*ssl)==0)                   
          return 0;
      if ((c=SSL_set_fd((*ssl), sock))!=1) 
          return 0;
  }
  c = SSL_accept(*ssl);
  if (c==1) {
      //SSL_set_session_id_context((*ssl), "webxsctx", 5);
      return 1;
  }
  return c;
}

char xs_sslize_connect(SSL **ssl, SSL_CTX *s, int sock) {
  if (ssl==0) return 0;
  SSL_CTX_set_verify(s, 0, 0);
  return ((*ssl) = SSL_new(s)) != NULL &&
    SSL_set_fd((*ssl), sock) == 1 && 
    //SSL_set_session_id_context ((*ssl), "webxsctx", 5)==1 &&
    SSL_connect((*ssl)) == 1;
}

char xs_SSL_free(SSL **ssl) {
  if (ssl==0 || *ssl==0) return 0;
  SSL_shutdown((*ssl));
  SSL_free((*ssl));
  (*ssl) = NULL;
  return 1;
}

// Return OpenSSL error message
const char *xs_SSL_error(SSL* ssl) {
  unsigned long err;
  err = ERR_get_error();
  if (err==0 && ssl) err = SSL_get_error(ssl, 0);
  return err == 0 ? "" : ERR_error_string(err, NULL);
}


//
// Each element consists of <the length of the string><string> - from chromium project flip_server:
// https://chromium.googlesource.com/chromium/src/+/master/net/tools/flip_server/
//
//#define NEXT_PROTO_STRING   "\x06spdy/3" "\x08http/1.1" "\x08http/1.0"
#define NEXT_PROTO_STRING   "\x08http/1.1" "\x08http/1.0"

static int xs_ssl_set_npn_callback(SSL* s, const unsigned char** data, unsigned int* len, void* arg) {
  (void)s; (void)arg;
  *data = (const unsigned char*)NEXT_PROTO_STRING;
  *len = strlen(NEXT_PROTO_STRING);
  return 0;
}

SSL_CTX*  xs_SSL_groom_CTX(SSL_CTX *ssl_ctx) {
  if (ssl_ctx==0) return 0;
  if (SSL_CTX_ctrl!=0) {
    SSL_CTX_set_options (ssl_ctx, SSL_OP_ALL|SSL_OP_NO_SSLv2);
    SSL_CTX_set_options (ssl_ctx, SSL_OP_NO_COMPRESSION); //to address CRIME attacks :/ (but also saves on RAM! :))
    SSL_CTX_set_mode    (ssl_ctx, SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_mode    (ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
    SSL_CTX_set_mode    (ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);
  }
  //SSL_CTX_set_session_id_context(ssl_ctx, "webxsctx", 5);
  /*
  if (SSL_CTX_set_next_protos_advertised_cb!=0) {
    SSL_CTX_set_next_protos_advertised_cb(ssl_ctx, xs_ssl_set_npn_callback, 0);
  }
  */
  return ssl_ctx;
}
SSL_CTX*        xs_SSL_newCTX_server ()             {return xs_SSL_groom_CTX(SSL_CTX_new(SSLv23_server_method()));}
SSL_CTX*        xs_SSL_newCTX_client ()             {return xs_SSL_groom_CTX(SSL_CTX_new(SSLv23_client_method()));}
void            xs_SSL_freeCTX (SSL_CTX* sslctx)    {if (sslctx) SSL_CTX_free (sslctx);}


void xs_SSL_locking_callback(int mode, int mutex_num, const char *file, int line) {
  line=line;  file=file; //stop GCC/CLANG etc from complaining...
  if (mode & 1) {  // 1 is CRYPTO_LOCK
    (void) pthread_mutex_lock(&xs_SSL_mutexes[mutex_num]);
  } else {
    (void) pthread_mutex_unlock(&xs_SSL_mutexes[mutex_num]);
  }
}


unsigned long xs_SSL_id_callback(void)      {return (unsigned long) pthread_self();}

int xs_SSL_set_certs(SSL_CTX* sslctx, char* privateKeyPem, char* certPem, char* certChainPem) {
    int ret=0;
    if (sslctx==0) return -1;
    if (certPem &&          SSL_CTX_use_certificate_file        (sslctx, certPem,       1) == 0) ret = -2;
    if (privateKeyPem &&    SSL_CTX_use_PrivateKey_file         (sslctx, privateKeyPem, 1) == 0) ret = -3;
    if (certChainPem &&     SSL_CTX_use_certificate_chain_file  (sslctx, certChainPem)     == 0) ret = -4;
    return ret;
}


// Dynamically load SSL library. Set up sslHolder_CTX.
int xs_SSL_initialize() {//, const char *pem, int (*init_ssl)(void *ssl_context, void *user_data), void* user_data) {
  int i, size;

  //already initialized?
  if (xs_atomic_inc(xs_SSL_refcount)!=0) return 0;

#if !defined(_xs_NO_SSL_DL_)
  if ((xs_SSL_lib=dlopen(xs_SSL_LIB, RTLD_LAZY))==NULL ||
      (xs_crypto_lib=dlopen(xs_CRYPTO_LIB, RTLD_LAZY))==NULL ||
      xs_LoadDLL_Funcs(xs_SSL_lib, xs_SSL_sw) ||
      xs_LoadDLL_Funcs(xs_crypto_lib, xs_crypto_sw)) {
    if (xs_SSL_lib) dlclose (xs_SSL_lib);       
    if (xs_crypto_lib) dlclose (xs_crypto_lib);
    xs_SSL_lib = xs_crypto_lib = 0;
    xs_atomic_dec(xs_SSL_refcount);
    return -1;
  }
#endif // _xs_NO_SSL_DL_

  // Initialize SSL library
  SSL_library_init();
  SSL_load_error_strings();

  // Initialize locking callbacks, needed for thread safety.
  // http://www.openssl.org/support/faq.html#PROG1
  size = sizeof(pthread_mutex_t) * CRYPTO_num_locks();
  if ((xs_SSL_mutexes = (pthread_mutex_t *) malloc((size_t)size)) == NULL)
    return -4;

  for (i = 0; i < CRYPTO_num_locks(); i++)
    pthread_mutex_init(&xs_SSL_mutexes[i], NULL);

  CRYPTO_set_locking_callback(&xs_SSL_locking_callback);
  CRYPTO_set_id_callback(&xs_SSL_id_callback);

  return 0;
}

void xs_SSL_uninitialize() {
  int i;
  if (xs_atomic_dec(xs_SSL_refcount)!=1) return;
  CRYPTO_set_locking_callback(NULL);
  for (i = 0; i < CRYPTO_num_locks(); i++) {
    pthread_mutex_destroy(&xs_SSL_mutexes[i]);
  }
  CRYPTO_set_locking_callback(NULL);
  CRYPTO_set_id_callback(NULL);

  if (xs_SSL_mutexes) {
    free(xs_SSL_mutexes);
    xs_SSL_mutexes = NULL;
  }
#if !defined(_xs_NO_SSL_DL_)
  if (xs_SSL_lib) dlclose (xs_SSL_lib);     
  if (xs_crypto_lib) dlclose (xs_crypto_lib);
  xs_SSL_lib = xs_crypto_lib = 0;
#endif
}

#endif // _xs_SSL_IMPL_
#endif // _xs_IMPLEMENTATION_

