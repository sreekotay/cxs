// This file is part of the Mongoose project, http://code.google.com/p/mongoose
// It implements an online chat server. For more details,
// see the documentation on the project web site.
// To test the application,
// 1. type "make" in the directory where this file lives
// 2. point your browser to http://127.0.0.1:8081
#ifdef WIN32
#define FD_SETSIZE 1024
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>
#include <direct.h>
#include <fcntl.h>
#else
#if __TINYC__
#else
#define FD_SETSIZE 4096
#endif
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>


//#include <pthread.h>
#include "xs/xs_posix_emu.h"
#include "xs/khash.h"
#include "xs/xs_queue.h"
#include "xs/xs_atomic.h"
#include "xs/xs_socket.h"
#include "xs/xs_connection.h"
#include "xs/xs_printf.h"
#include "xs/xs_ssl.h"
#include "xs/xs_sha1.h"
#include "xs/xs_logger.h"
#include "xs/xs_fileinfo.h"
#include "xs/xs_server.h"
#include "xs/xs_startup.h"


int gexit = 0;


#define myexit(a)  do {printf ("[exit %s %ld]\n", __FILE__, (long)__LINE__); exit(a);} while(0)


#if 0
#include "mongoose.h"

#define MAX_USER_LEN  20
#define MAX_MESSAGE_LEN  100
#define MAX_MESSAGES 5
#define MAX_SESSIONS 2
#define SESSION_TTL 120

static const char *authorize_url = "/authorize";
static const char *login_url = "/login.html";
static const char *ajax_reply_start =
  "HTTP/1.1 200 OK\r\n"
  "Cache: no-cache\r\n"
 // "Connection: close\r\n"
  "Content-Type: application/x-javascript\r\n";
  //"\r\n";

// Describes single message sent to a chat. If user is empty (0 length),
// the message is then originated from the server itself.
struct message {
  long id;                     // Message ID
  char user[MAX_USER_LEN];     // User that have sent the message
  char text[MAX_MESSAGE_LEN];  // Message text
  time_t timestamp;            // Message timestamp, UTC
};

// Describes web session.
struct session {
  char session_id[33];      // Session ID, must be unique
  char random[20];          // Random data used for extra user validation
  char user[MAX_USER_LEN];  // Authenticated user
  time_t expire;            // Expiration timestamp, UTC
};

static struct message messages[MAX_MESSAGES];  // Ringbuffer for messages
static struct session sessions[MAX_SESSIONS];  // Current sessions
static long last_message_id;

// Protects messages, sessions, last_message_id
static pthread_rwlock_t rwlock;// = PTHREAD_RWLOCK_INITIALIZER;

// Get session object for the connection. Caller must hold the lock.
static struct session *get_session(const struct mg_connection *conn) {
  int i;
  const char *cookie = mg_get_header(conn, "Cookie");
  char session_id[33];
  time_t now = time(NULL);
  mg_get_cookie(cookie, "session", session_id, sizeof(session_id));
  for (i = 0; i < MAX_SESSIONS; i++) {
    if (sessions[i].expire != 0 &&
        sessions[i].expire > now &&
        strcmp(sessions[i].session_id, session_id) == 0) {
      break;
    }
  }
  return i == MAX_SESSIONS ? NULL : &sessions[i];
}

static void get_qsvar(const struct mg_request_info *request_info,
                      const char *name, char *dst, size_t dst_len) {
  const char *qs = request_info->query_string;
  mg_get_var(qs, strlen(qs == NULL ? "" : qs), name, dst, dst_len);
}

// Get a get of messages with IDs greater than last_id and transform them
// into a JSON string. Return that string to the caller. The string is
// dynamically allocated, caller must free it. If there are no messages,
// NULL is returned.
static char *messages_to_json(long last_id) {
  const struct message *message;
  int max_msgs, len;
  char buf[sizeof(messages)];  // Large enough to hold all messages

  // Read-lock the ringbuffer. Loop over all messages, making a JSON string.
  pthread_rwlock_rdlock(&rwlock);
  len = 0;
  max_msgs = sizeof(messages) / sizeof(messages[0]);
  // If client is too far behind, return all messages.
  if (last_message_id - last_id > max_msgs) {
    last_id = last_message_id - max_msgs;
  }
  for (; last_id < last_message_id; last_id++) {
    message = &messages[last_id % max_msgs];
    if (message->timestamp == 0) {
      break;

    }
    // buf is allocated on stack and hopefully is large enough to hold all
    // messages (it may be too small if the ringbuffer is full and all
    // messages are large. in this case asserts will trigger).
    len += xs_sprintf(buf + len, sizeof(buf) - len,
        "{user: '%s', text: '%s', timestamp: %lu, id: %lu},",
        message->user, message->text, message->timestamp, message->id);
    assert(len > 0);
    assert((size_t) len < sizeof(buf));
  }
  pthread_rwlock_rdunlock(&rwlock);

  return len == 0 ? NULL : strdup(buf);
}

// If "callback" param is present in query string, this is JSONP call.
// Return 1 in this case, or 0 if "callback" is not specified.
// Wrap an output in Javascript function call.
static int handle_jsonp(struct mg_connection *conn,
                        const struct mg_request_info *request_info,
						char *cb, int cbsize) {
  get_qsvar(request_info, "callback", cb, cbsize);

  return cb[0] == '\0' ? 0 : 1;
}

// A handler for the /ajax/get_messages endpoint.
// Return a list of messages with ID greater than requested.
static void ajax_get_messages(struct mg_connection *conn,
                              const struct mg_request_info *request_info) {
  char last_id[32], *json, cb[64];
  int is_jsonp;

  mg_printf_header(conn, "%s", ajax_reply_start);
  mg_printf_header(conn, "%s", "Transfer-Encoding: chunked\r\n\r\n");
  is_jsonp = handle_jsonp(conn, request_info, cb, sizeof(cb));

  get_qsvar(request_info, "last_id", last_id, sizeof(last_id));
  if ((json = messages_to_json(strtoul(last_id, NULL, 10))) != NULL) {
    if (is_jsonp)	mg_printf_chunked(conn, "%s([%s])", cb, json);
	else			mg_printf_chunked(conn, "[%s]", json);
    free(json);
  } else if (is_jsonp) {
	  mg_printf_chunked (conn, "%s()", cb);
  }
  mg_printf_chunked (conn, ""); //terminate chunk
}

// Allocate new message. Caller must hold the lock.
static struct message *new_message(void) {
  static int size = sizeof(messages) / sizeof(messages[0]);
  struct message *message = &messages[last_message_id % size];
  message->id = last_message_id++;
  message->timestamp = time(0);
  return message;
}

static void my_strlcpy(char *dst, const char *src, size_t len) {
  strncpy(dst, src, len);
  dst[len - 1] = '\0';
}

// A handler for the /ajax/send_message endpoint.
static void ajax_send_message(struct mg_connection *conn,
                              const struct mg_request_info *request_info) {
  struct message *message;
  struct session *session;
  char text[sizeof(message->text) - 1], cb[64];
  int is_jsonp;

  mg_printf(conn, "%s", ajax_reply_start);
  mg_printf_header(conn, "%s", "Transfer-Encoding: chunked\r\n\r\n");
  is_jsonp = handle_jsonp(conn, request_info, cb, sizeof(cb));

  get_qsvar(request_info, "text", text, sizeof(text));
  if (text[0] != '\0') {
    // We have a message to store. Write-lock the ringbuffer,
    // grab the next message and copy data into it.
    pthread_rwlock_wrlock(&rwlock);
    message = new_message();
    // TODO(lsm): JSON-encode all text strings
    session = get_session(conn);
    assert(session != NULL);
    my_strlcpy(message->text, text, sizeof(text));
    my_strlcpy(message->user, session->user, sizeof(message->user));
    pthread_rwlock_wrunlock(&rwlock);
  }

   if (is_jsonp)	mg_printf_chunked(conn, "%s(%s)", cb, text[0] == '\0' ? "false" : "true");
  else              mg_printf_chunked(conn, "%s", text[0] == '\0' ? "false" : "true");

   mg_printf_chunked (conn, ""); //terminate chunk

}

// Redirect user to the login form. In the cookie, store the original URL
// we came from, so that after the authorization we could redirect back.
static void redirect_to_login(struct mg_connection *conn,
                              const struct mg_request_info *request_info) {
  mg_printf(conn, "HTTP/1.1 302 Found\r\n"
      "Set-Cookie: original_url=%s\r\n"
      "Location: %s\r\n\r\n",
      request_info->uri, login_url);
}

// Return 1 if username/password is allowed, 0 otherwise.
static int check_password(const char *user, const char *password) {
  // In production environment we should ask an authentication system
  // to authenticate the user.
  // Here however we do trivial check that user and password are not empty
  return (user[0] && password[0]);
}

// Allocate new session object
static struct session *new_session(void) {
  int i;
  time_t now = time(NULL);
  pthread_rwlock_wrlock(&rwlock);
  for (i = 0; i < MAX_SESSIONS; i++) {
    if (sessions[i].expire == 0 || sessions[i].expire < now) {
      sessions[i].expire = time(0) + SESSION_TTL;
      break;
    }
  }
  pthread_rwlock_wrunlock(&rwlock);
  return i == MAX_SESSIONS ? NULL : &sessions[i];
}

// Generate session ID. buf must be 33 bytes in size.
// Note that it is easy to steal session cookies by sniffing traffic.
// This is why all communication must be SSL-ed.
static void generate_session_id(char *buf, const char *random,
                                const char *user) {
  xs_md5(buf, random, user, NULL);
}

static void send_server_message(const char *fmt, ...) {
  va_list ap;
  struct message *message;

  pthread_rwlock_wrlock(&rwlock);
  message = new_message();
  message->user[0] = '\0';  // Empty user indicates server message
  va_start(ap, fmt);
  vsnprintf(message->text, sizeof(message->text), fmt, ap);
  va_end(ap);

  pthread_rwlock_wrunlock(&rwlock);
}

// A handler for the /authorize endpoint.
// Login page form sends user name and password to this endpoint.
static void authorize(struct mg_connection *conn,
                      const struct mg_request_info *request_info) {
  char user[MAX_USER_LEN], password[MAX_USER_LEN];
  struct session *session;

  // Fetch user name and password.
  get_qsvar(request_info, "user", user, sizeof(user));
  get_qsvar(request_info, "password", password, sizeof(password));

  if (check_password(user, password) && (session = new_session()) != NULL) {
    // Authentication success:
    //   1. create new session
    //   2. set session ID token in the cookie
    //   3. remove original_url from the cookie - not needed anymore
    //   4. redirect client back to the original URL
    //
    // The most secure way is to stay HTTPS all the time. However, just to
    // show the technique, we redirect to HTTP after the successful
    // authentication. The danger of doing this is that session cookie can
    // be stolen and an attacker may impersonate the user.
    // Secure application must use HTTPS all the time.
    my_strlcpy(session->user, user, sizeof(session->user));
    xs_sprintf(session->random, sizeof(session->random), "%d", rand());
    generate_session_id(session->session_id, session->random, session->user);
    send_server_message("<%s> joined", session->user);
    mg_printf(conn, "HTTP/1.1 302 Found\r\n"
        "Set-Cookie: session=%s; max-age=3600; http-only\r\n"  // Session ID
        "Set-Cookie: user=%s\r\n"  // Set user, needed by Javascript code
        "Set-Cookie: original_url=/; max-age=0\r\n"  // Delete original_url
        "Location: /\r\n\r\n",
        session->session_id, session->user);
  } else {
    // Authentication failure, redirect to login.
    redirect_to_login(conn, request_info);
  }
}

// Return 1 if request is authorized, 0 otherwise.
static int is_authorized(const struct mg_connection *conn,
                         const struct mg_request_info *request_info) {
  struct session *session;
  char valid_id[33];
  int authorized = 0;

  // Always authorize accesses to login page and to authorize URI
  if (!strcmp(request_info->uri, login_url) ||
      !strcmp(request_info->uri, authorize_url)) {
    return 1;
  }

  pthread_rwlock_rdlock(&rwlock);
  if ((session = get_session(conn)) != NULL) {
    generate_session_id(valid_id, session->random, session->user);
    if (strcmp(valid_id, session->session_id) == 0) {
      session->expire = time(0) + SESSION_TTL;
      authorized = 1;
    }
  }
  pthread_rwlock_rdunlock(&rwlock);

  return authorized;
}

static void redirect_to_ssl(struct mg_connection *conn,
                            const struct mg_request_info *request_info) {
  const char *p, *host = mg_get_header(conn, "Host");
  if (host != NULL && (p = strchr(host, ':')) != NULL) {
    mg_printf(conn, "HTTP/1.1 302 Found\r\n"
              "Location: https://%.*s:8082/\r\n\r\n",
              (int) (p - host), host, request_info->uri);
  } else {
    mg_printf(conn, "%s", "HTTP/1.1 500 Error\r\n\r\nHost: header is not set");
  }
}

static int begin_request_handler(struct mg_connection *conn) {
  const struct mg_request_info *request_info = mg_get_request_info(conn);
  int processed = 1;

  if (strcmp(request_info->uri, "/") == 0 && !request_info->is_ssl ) {
    redirect_to_ssl(conn, request_info);
  } else if (strcmp(request_info->uri, "/") == 0 && !is_authorized(conn, request_info)) {
    redirect_to_login(conn, request_info);
  } else if (strcmp(request_info->uri, authorize_url) == 0) {
    authorize(conn, request_info);
  } else if (strcmp(request_info->uri, "/ajax/get_messages") == 0) {
    ajax_get_messages(conn, request_info);
  } else if (strcmp(request_info->uri, "/ajax/send_message") == 0) {
    ajax_send_message(conn, request_info);
  } else {
    // No suitable handler found, mark as not processed. Mongoose will
    // try to serve the request.
    processed = 0;
  }
  return processed;
}


static void websocket_ready_handler(struct mg_connection *conn) {
  static const char *message = "server ready";
  mg_websocket_write(conn, WEBSOCKET_OPCODE_TEXT, message, strlen(message));
}

// Arguments:
//   flags: first byte of websocket frame, see websocket RFC,
//          http://tools.ietf.org/html/rfc6455, section 5.2
//   data, datalen: payload data. Mask, if any, is already applied.
static int websocket_data_handler(struct mg_connection *conn, int flags,
                                  char *data, size_t datalen) {
  (void) flags; // Unused
  mg_websocket_write(conn, WEBSOCKET_OPCODE_TEXT, data, datalen);

  // Returning zero means stoping websocket conversation.
  // Close the conversation if client has sent us "exit" string.
  return memcmp(data, "exit", 4);
}

static const char *options[] = {
  "document_root", "html",
  "listening_ports", "8081,8082s",
  "ssl_certificate", "ssl_cert.pem",
  "num_threads", "5",
  NULL
};
#endif //0

int gtotaldl=1000;
int gconcurrent=1;
xs_atomic gtotalconn=0, gtotalbytes=0;
clock_t gt;
double gtt;

double xs_time (void) {
    struct timespec ts;
    clock_gettime (CLOCK_REALTIME, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}



struct my_download {
	xs_conn*	conn;
	xs_atomic	asked, cur, total;
	int			needed, bytes;
	char*		request;
};


int gpipeline=0;
int sbc=0,slc=0;
int xs_webxs_cb (struct xs_async_connect* xas, int message, xs_conn* conn) {
	struct my_download* dl = (struct my_download*)xs_conn_getuserdata (conn);
	char buf[10240];
	int n, c, cc, err=0;
    int wcount = strlen(dl->request);

	switch (message) {
		case exs_Conn_Write:
		case exs_Conn_New:
			if (gpipeline==0) {
				cc = xs_conn_printf (conn, "%s", dl->request);
				break;
			}
			c = cc = 0;
			//printf ("write start==: n:%d of %d,  bytes:%d errno:%d revents:%d\n", dl->asked, dl->total, c, errno, cc);
			for (n=dl->asked; n<dl->total; n++) {
				if (1) {//cc=xs_sock_avail(xs_conn_getsock (conn), POLLOUT) & POLLOUT) {
					cc = xs_conn_printf (conn, "%s", dl->request);
					c += cc;
					if (cc!=wcount) {printf ("not %d: %d\n", wcount, cc); 
						break;}
				} else 
					{break;}//printf ("unable to write to socket\n"); break;}
				if (xs_conn_error(conn)==exs_Error_WriteBusy)
					break;
				//printf ("write %d %d\n", xs_conn_printf (conn, "%s", dl->request), n);
			}
			dl->asked = n;
			//printf ("write end=====: n:%d of %d,  bytes:%d errno:%d revents:%d\n", dl->asked, dl->total, c, errno, cc);
		//	if (n<dl->total) 
			//	err = exs_Conn_Write;
		break;

		case exs_Conn_Read:
			if (0) {
				//printf ("start ----------------------------------------------------------- %d\n", gtotalbytes);
				do {
					n = xs_conn_read (conn, buf, sizeof(buf)-1, 0);//recv (xs_conn_getsock (conn), buf, sizeof(buf)-1, 0);
					//n = mg_read (conn, buf, sizeof(buf)-1);
					if (n<=0) {
						cc = errno;//WSAGetLastError();
						break;
					}
					buf[n]=0;
					c = xs_atomic_add (gtotalbytes, n);
					//printf ("%s\n -------- out %ld\n", "", c);
					//printf ("%s", buf);
				} while (1);
				printf ("end ----------------------------------------------------------- %ld n:%d\n", gtotalbytes, n);
				if (n==0) {
					printf ("socket closed %d\n", cc);
					err = xs_webxs_cb(xas, exs_Conn_Close, conn);
                    myexit(2);
				}
				//if (dl->asked<dl->total)
				//	err = xs_webxs_cb(xas, exs_Conn_New, conn);
				break;
			}

			do {
                int rr;
				int osbc=sbc;
				static int totc=0;
                /*
                  if (conn->ssl)
                    xs_SSL_protocol (conn->ssl, &str, &len);
                    */
				cc=0;
				while ((n=xs_conn_httpread(conn, buf, sizeof(buf)-1, &rr))>0 || rr) {
                    if (n>0) {
					    buf[n]=0;
					    //totc+=printf ("%s", buf);
					    //printf ("block [%d]: %s", n, buf);
					    //printf ("n [%ld]: %d -- dlb[%d] tb[%ld]\n", dl->cur+cc, n, dl->bytes, gtotalbytes);
					    xs_atomic_add (gtotalbytes, n);
					    if (n>100) 
						    n=n;
					    sbc += n;
					    dl->bytes += n;
                    }
					if (xs_conn_state(conn)==exs_Conn_Complete) {
						//dl->bytes=0;
						cc++;
						//n=mg_read(conn, buf, sizeof(buf)-1);
						//break;
					} //else printf ("not %d\n", xs_conn_status(conn));
				}

              //  if (xs_conn_state(conn)==exs_Conn_Complete)
                //    cc++;


				//if (dl->asked<dl->total)
				//	err = xs_webxs_cb(xas, exs_Conn_New, conn);

				if (cc==0 && xs_conn_state(conn)!=exs_Conn_Complete) {
					//printf ("not_also %d\n", xs_conn_status(conn));
					if (n==0) {
						xs_logger_info ("host closed connection %d", dl->bytes); 
						xs_conn_dec (conn);
						err = exs_Conn_Close;
						break;
					}
					continue;
				}

				//done!
				//cc=1;
				if (cc==0) {
					//printf ("cc 0 -- n:%d\n", n);
					//printf ("not_third %d n[%d]\n", xs_conn_status(conn), n);
					continue;
					cc=1;
				}
				if (cc*100 != sbc)
					c=c;
				sbc = 0;
				slc++;

				if (cc==0 || cc>1) 
					cc=cc;
				c = xs_atomic_add (dl->cur, cc);
				//dl->bytes = 0;
				if (c<=dl->total) {
					if (gpipeline==0 && c<dl->total) 
						xs_conn_printf (conn, "%s", dl->request);
					if (0 || (gtotaldl/10 && gtotalconn%(gtotaldl/10))==0)
						printf ("prog %d %ld [%ld] cc[%d]\n", dl->bytes, gtotalbytes, dl->cur, cc);
					if (xs_atomic_add (gtotalconn, cc)==gtotaldl-cc) {
						double tt = xs_time() - gtt;

						printf ("total %d %ld [%ld] ---- time [%g] rps[%g]\n", dl->bytes, gtotalbytes, gtotalconn, tt, (float)gtotalconn/tt);
						printf ("stopped\n");
						xs_async_destroy(xas);
						n=0;
                        gexit=1;
					}
				} else {
					printf ("close conn\n");
					xs_conn_dec (conn);
					err=exs_Conn_Close;
                    break;
				}
				
			} while (n>0);
		break;

		case exs_Conn_Error:
		case exs_Conn_Close:
			//nothing yet
			printf ("error...%d\n", errno);
		break;

	}

	if (gexit) {
		printf ("quitting....\n");
		xs_async_destroy(xas);
	}
	return err;
}

int do_div (int t, int d, int n) {
	return (t/d) + (n==0 ? t-(t/d)*d: 0);
}
struct xs_async_connect*  launch_connects (const char* host, int port, char* request, int concount, int total) {
  struct xs_async_connect* xas=0;
  int n, err;
  char ebuf[256]="";
  struct my_download* dl = calloc(sizeof(*dl)*concount, 1);
  for (n=0; n<concount; n++) {
	err = xs_conn_open (&dl[n].conn, host, port, 0);
	if (err) printf ("host: %s port: %d, error %d se:%d\n", host, port, err, xs_sock_err());
	dl[n].bytes = 0;
	dl[n].request = request;
	dl[n].cur = 0;
	dl[n].total = do_div(total, concount, n);
	xs_conn_setuserdata (dl[n].conn, dl+n);
	if (ebuf[0]==0)
	  //xs_Pollfd_push (gxp_dl, mg_get_socket(dl[n].conn), dl+n); 
	  xas = xs_async_read (xas, dl[n].conn, xs_webxs_cb);
  }
  printf ("------------------------------------- round\n");
  if (0) {
	  while (xs_async_active(xas)) {sleep(1);}
	//xs_Pollfd_run (gxp_dl);
  }
  return xas;
  //free(dl);
  //xs_async_destroy(xas);
}





#ifdef _old_file_stuff_

KHASH_MAP_INIT_STR(stat, xs_fileinfo);
khash_t(stat) *g_statHash = 0;
pthread_mutex_t g_stat_mutex;
xs_atomic g_statRead = 0;
xs_queue g_statFix;
void my_statFixProc (xs_queue* qs, char** pathPtr, void* privateData);
void my_statHashInit() {
	if (g_statHash) return;
	pthread_mutex_init (&g_stat_mutex, NULL);
	pthread_mutex_lock (&g_stat_mutex);
	g_statHash = kh_init(stat);
	pthread_mutex_unlock (&g_stat_mutex);
	xs_queue_create (&g_statFix, sizeof(char*), 1024*10, (xs_queue_proc)my_statFixProc, NULL);
	xs_queue_launchthreads (&g_statFix, 2, 0);
}
void my_statHashDestroy() {
	khash_t(stat) *statHash = g_statHash;
	if (statHash==0) return;
	pthread_mutex_lock (&g_stat_mutex);
	g_statHash = 0;
	kh_destroy(stat, statHash);
	pthread_mutex_unlock (&g_stat_mutex);
	pthread_mutex_destroy (&g_stat_mutex);
	xs_queue_destroy (&g_statFix);
}


void my_statFixProc (xs_queue* qs, char** pathPtr, void* privateData) {
	khiter_t k;
	FILE* fh;
	xs_fileinfo fp, *f, fp2;
	char *filedata;
	char* path = *pathPtr;

	while (xs_atomic_swap (g_statRead, 0, 1)!=0) {sched_yield();}
	k = kh_get(stat,g_statHash,path);
	f = (k==kh_end(g_statHash)) ? 0 : (&kh_val(g_statHash, k));
	if (f && xs_atomic_swap (f->status, 0, 100)!=0) {
		g_statRead = 0;
		xs_queue_add(&g_statFix, (void*)&path);
		printf ("Refresh Stat Busy.  Retry %s\n", path);
		return;
	}
	if (f) {
		f->status = 0;
		fp2 = *f;
		f = &fp2;
	}
	g_statRead = 0;

	//printf ("Refresh Stat %s\n", path);
	if (f && xs_stat(path, &fp)==0) {
		if (fp.modification_time != f->modification_time || 
			fp.size != f->size || (f->data==0 && fp.size<300000 && fp.is_directory==0)) {
			//pthread_mutex_lock (&g_stat_mutex);
			filedata = f->data;
			fp.status = f->status;
			//pthread_mutex_unlock (&g_stat_mutex);
			if (filedata) free(filedata);
			printf ("Refreshed %s\n", path);
			fp.data = (char*)malloc((int)fp.size);
			fh =fp. data ? xs_fopen(path, "rb") : NULL;
			if (fh) {
				if (fread (fp.data, 1, (int)fp.size, fh)!=(int)fp.size) {
					free(fp.data); fp.data=0;
				}
				fclose(fh);
			} else {free(fp.data); fp.data=0;}
			//pthread_mutex_lock (&g_stat_mutex);
			*f = fp;
			fp2.status = 0;
			while (xs_atomic_swap (g_statRead, 0, 1)!=0) {sched_yield();}
			k = kh_get(stat,g_statHash,path);
			f = (k==kh_end(g_statHash)) ? 0 : (&kh_val(g_statHash, k));
			if (f && xs_atomic_swap (f->status, 0, 100)!=0) {
				g_statRead = 0;
				xs_queue_add(&g_statFix, (void*)&path);
				printf ("Refresh Stat Busy.  Retry %s\n", path);
				return;
			}
			*f = fp2;
			g_statRead = 0;
			//pthread_mutex_unlock (&g_stat_mutex);
		}
	} else if (f) {
		//could not read file
		//pthread_mutex_lock (&g_stat_mutex);
		f->stat_ret = 1;
		//pthread_mutex_unlock (&g_stat_mutex);
	}
	free(path);
}
xs_fileinfo* my_statHashGet(const char* path) {
	khiter_t k;
	FILE* fh;
	xs_fileinfo* f, fp, fp2, *of;
	int ret;
	time_t ct = time(0);
	char *filedata;

	while (xs_atomic_swap (g_statRead, 0, 1)!=0) {sched_yield();}
	k = kh_get(stat,g_statHash,path);
	f = (k==kh_end(g_statHash)) ? 0 : (&kh_val(g_statHash, k));
	//g_statRead = 0;

	if (f==0) {
		pthread_mutex_lock (&g_stat_mutex);
		//while (xs_atomic_swap (g_statRead, 1, 2)!=0) {sched_yield();}
		k = kh_put(stat,g_statHash,path,&ret);
		f = &kh_val(g_statHash, k);
		if (f) {
			if (ret!=0) {
				f->status=2;
				kh_key(g_statHash, k) = xs_strdup(path);
				memset(f, 0, sizeof(*f));
				f->status=0;
			} else f->status = 0;
		}
		//g_statRead = 0;
		pthread_mutex_unlock (&g_stat_mutex);
	} else ret = 0;
	//		printf ("Stat %s %ld, %ld\n", path, (time_t)(ct-f->check_time), (time_t)5*CLOCKS_PER_SEC);

	if (f) {
		of = f;
		fp2 = *f;
		f = &fp2;
	}

	//is the key new?
	if (ret!=0) {// || ct-f->check_time > 5) {
		//is the key uninitialized?
		printf ("stat refresh yield 10 status:%ld\n", f->status); 
		f->check_time = ct;
		if (xs_stat(path, &fp)==0) {
			//g_statRead = 0;
			printf ("Stat %s\n", path);
			f->status = 2;
			if (fp.modification_time != f->modification_time || 
				fp.size != f->size || (f->data==0 && fp.size<300000 && fp.is_directory==0)) {
				//pthread_mutex_lock (&g_stat_mutex);
				filedata = f->data;
				fp.status = f->status;
				*f = fp;
				f->check_time = 0;//ct;
				//pthread_mutex_unlock (&g_stat_mutex);
				if (filedata) free(filedata);
				printf ("Read %s\n", path);
				fp.data = 0;//malloc((int)fp.size);
				fh = fp.data ? xs_fopen(path, "rb") : NULL;
				if (fh) {
					if (fread (fp.data, 1, (int)fp.size, fh)!=(int)fp.size) {
						free(fp.data); fp.data=0;
					}
					fclose(fh);
				} else {free(fp.data); fp.data=0;}
				//pthread_mutex_lock (&g_stat_mutex);
				f->data = fp.data;
				//pthread_mutex_unlock (&g_stat_mutex);
			}

			//while (xs_atomic_swap (g_statRead, 0, 1)!=0) {sched_yield();}
			k = kh_get(stat,g_statHash,path);
			f = (k==kh_end(g_statHash)) ? 0 : (&kh_val(g_statHash, k));
			if (f && xs_atomic_swap (f->status, 0, 10)!=0) {
				g_statRead = 0;
				xs_queue_add(&g_statFix, (void*)&path);
				printf ("Refresh Stat Busy.  Retry %s\n", path);
				return f;
			}
			*f = fp2;
			g_statRead = 0;

			while (xs_atomic_swap(f->status,2,0)!=2) {printf ("stat refresh yield 3\n"); sched_yield();}
			path = xs_strdup(path);
			xs_queue_add(&g_statFix, (void*)&path);
		} else {
			//while (xs_atomic_swap (g_statRead, 0, 1)!=0) {sched_yield();}
			k = kh_get(stat,g_statHash,path);
			f = (k==kh_end(g_statHash)) ? 0 : (&kh_val(g_statHash, k));
			if (f) f->stat_ret = -1;
			g_statRead = 0;
		}
	} else if (1 && ct-f->check_time > 1) {
		if (of) of->check_time = ct;
		g_statRead = 0;
		path = xs_strdup(path);
		xs_queue_add(&g_statFix, (void*)&path);
	}
	g_statRead = 0;
	//printf ("%s key[%s] hash[%d] ptr[%d] ret[%d] c-ret [%d] dir[%d]\n", path, kh_key(g_statHash, k), kh_str_hash_func(path), (int)path, ret, f->stat_ret, f->is_directory);
	return f;
}
#endif


char msghdr[] = 
			"HTTP/1.1 200 OK\r\n"
			"Content-Length: 100\r\n"
			"Connection: keep-alive\r\n"
	//		"Accept-Ranges: bytes\r\n"
	//		"Server: webxs\r\n"
	//		"Date: today\r\n"
	//		"Etag: \"bytes\"\r\n"
            "Content-Type: text/plain\r\n\r\n";
char msg[] =
			"100xxxxxxx"
			"xxxxxxxxxx"
			"xxxxxxxxxx"
			"xxxxxxxxxx"
			"xxxxxxxxxx"
			"xxxxxxxxxx"
			"xxxxxxxxxx"
			"xxxxxxxxxx"
			"xxxxxxxxxx"
			"xxxxxxxxx0"
				;

#ifdef WIN32
#define INT64_FMT		"I64d"
#else
#define INT64_FMT		"lld"
#endif

#if __TINYC__
#define __thread
#endif

//const char* xs_server_name(xs_server_ctx* ctx) {return ctx && ctx->server_name[0] ? ctx->server_name : "webxs/0.5";}






int dodecompress(char* path);
int docompress(char *path);

int main(int argc, char *argv[]) {
#if 1
	char request[1024];
	//char request[]="GET /100.html HTTP/1.0\r\nHost: google.com\r\n\r\n";
	int port=80, tc=1, n, i, err, accesslog=0;
	xs_conn *conn=0, *conn_ssl=0;
	xs_conn *conn6=0, *conn_ssl6=0;
	xs_uri* u=0;
#ifdef WIN32
	WSADATA data;
	WSAStartup(MAKEWORD(2,2), &data);
#endif
#ifndef WIN32
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror(0);
        exit(1);
    }

#endif
    xs_startup(exs_Start_All, xs_server_terminate_all, 0);

	
	if (1) {
	  xs_uri* u;
	  if ((u=xs_uri_create("//google.com:20/23?#now", 0))!=0)
		  xs_uri_destroy(u);
	  if ((u=xs_uri_create("http://google.com:20/23?test#now", 0))!=0)
		  xs_uri_destroy(u);
	  if ((u=xs_uri_create("https://google.com", 0))!=0)
		  xs_uri_destroy(u);
	  if ((u=xs_uri_create("google.com/now", 0))!=0)
		  xs_uri_destroy(u);
	  if ((u=xs_uri_create("google.com/now", 1))!=0)
		  xs_uri_destroy(u);
	  if ((u=xs_uri_create("google.com", 1))!=0)
		  xs_uri_destroy(u);
	  if ((u=xs_uri_create("c://test.bat", 0))!=0)
		  xs_uri_destroy(u);
	}

    if (argc>2 && strcmp(argv[1],"-d")==0) {
        dodecompress (argv[2]);
        myexit(1);
    }

    if (argc>2 && strcmp(argv[1],"-c")==0) {
        docompress (argv[2]);
        myexit(1);
    }

    if (argc>1 && strcmp(argv[1],"-v")==0) {
        printf ("%s\n", xs_server_name());
        myexit(1);
    }
	
    if (argc>1 && strcmp(argv[1],"-a")==0) {
        accesslog = 1;
        printf ("access log on\n");
    }

	if (argc<2 || strcmp(argv[1],"-b")) {
#ifdef _old_file_stuff_
        my_statHashInit();
#endif
        if (accesslog==0) xs_logger_level(exs_Log_Error, exs_Log_Info);
       
       // broadcastmdns ();
										
		err=xs_conn_listen(&conn, 8080, 0, 1); //must bind v6 first???
        if (err) xs_logger_error ("listen v6 %d se:%d", err, xs_sock_err());
		if (1) {//err==0) {
			err = xs_conn_listen(&conn6, 8080, 0, 0);
			if (err) xs_logger_error ("listen v4 %d se:%d", err, xs_sock_err());
			err = 0;
		}
		if (err==0) {
			err=xs_conn_listen(&conn_ssl, 443, 1, 0);
			if (err==0)
				err = xs_SSL_set_certs(xs_conn_sslctx(conn_ssl),	"default_webxs_ssl_key.pem", 
																	"default_webxs_ssl_key.pem", 
																	"default_webxs_ssl_key.pem");
			if (err) xs_logger_error ("SSL init error");
			err = 0;
		}
		if (err==0) {
            xs_server_ctx *ctx = xs_server_create(".", argv[0]);
            /*
			xs_server_ctx ctx = {0};
			struct xs_async_connect* xas=xs_async_create(32);
			xs_strlcpy(ctx.server_name, xs_server_name(), sizeof(ctx.server_name));
			xs_path_setabsolute (ctx.document_root, sizeof(ctx.document_root), ".", argv[0]);
			gxas = xas;
            */
			xs_server_listen (ctx, conn);
			xs_server_listen (ctx, conn6);
            xs_server_listen (ctx, conn_ssl);
			while (xs_server_active(ctx)) {sleep(1);}
			//xs_async_destroy(xas);
			xs_logger_fatal ("---- done ----");
			/*
            xs_conn_destroy(conn);
			xs_conn_destroy(conn6);
			xs_conn_destroy(conn_ssl);
            */
            //sleep(1);
            //xs_logger_destroy();
            xs_server_destroy(ctx);
            xs_logger_counter_print();
			myexit(1);
		} else {
			xs_logger_fatal ("listen error %d", err);
           // xs_logger_destroy();
			myexit(2);
		}
	}
	
	
	i=1;
	while (i<argc) {
		if (strcmp(argv[i],"-p")==0)
			{if (i+1<argc) port=(int)atof(argv[++i]);}
		else if (strcmp(argv[i],"-b")==0)
			{}
		else if (strcmp(argv[i],"-pipeline")==0)
			{gpipeline=1;}
		else if (strcmp(argv[i],"-a")==0)
			{accesslog=1;}
		else if (strcmp(argv[i],"-t")==0)
			{if (i+1<argc) tc=(int)atof(argv[++i]);}
		else if (strcmp(argv[i],"-n")==0)
			{if (i+1<argc) gtotaldl=(int)atof(argv[++i]);}
		else if (strcmp(argv[i],"-c")==0)
			{if (i+1<argc) gconcurrent=(int)atof(argv[++i]);}
		else if ((u=xs_uri_destroy(u))==0) {
			printf ("b1\n");
			u = xs_uri_create(argv[i], ((argv[i])[0]!='/')*2);
			printf ("b2: host %s, path %s, port %d\n", u->host?u->host:"none", u->path?u->path:"none", u->port);
		}
		i++;
	}
	
    if (0) {
        xs_conn* conn;
        int n, rr;
        char buf[4096], msg[]="this is a simple message.";
        err = xs_conn_open (&conn, "127.0.0.1", 8080, 0);
        if (conn==0) myexit(2);
        //err = xs_conn_httprequest (conn, "www.google.com", "GET", "/");
        err = xs_conn_httpwebsocket (conn, "www.google.com", "/");
        err = xs_conn_header_done (conn, 1);
        err = xs_conn_write_websocket (conn, exs_WS_TEXT, msg, strlen(msg), 1);
        err = xs_conn_write_websocket (conn, exs_WS_TEXT, msg, strlen(msg), 1);
       // err = xs_conn_write_websocket (conn, exs_WS_CONNECTION_CLOSE, 0, 0, 1);
        while ((n=xs_conn_httpread (conn, buf, sizeof(buf)-1, &rr))!=0 && 
            xs_http_getint(xs_conn_getreq(conn), exs_Req_KeepAlive) && xs_conn_error(conn)==0) {//s=xs_conn_state(conn))!=exs_Conn_Complete) {// && s!=exs_Conn_Websocket) {
            if (n>0) {
                buf[n]=0;
                printf ("%s", buf);
            }
        };
        xs_conn_destroy(conn);
            //xs_conn_httpwebsocket (xs_conn* conn, const char* host, const char* path) 
        myexit(1);
    }
	
	if (tc>gconcurrent) {
		printf ("reducing thread count to concurrent count %d->%d\n", tc, gconcurrent);
		tc = gconcurrent;
	}
	
	gt = clock();
	gtt = xs_time();
	xs_sprintf (request, sizeof(request), "GET %s HTTP/1.1\r\nConnection: keep-alive\r\nHost: %s\r\n\r\n", 
                                            (u&&u->path ? u->path : "/100.html"), 
                                            u->host ? u->host : "127.0.0.1");
	printf ("Request: %s\n", request);
	for (n=0; n<tc; n++) {
		launch_connects(u&&u->host? u->host : "127.0.0.1", u&&u->port ? u->port : port, request, do_div(gconcurrent, tc, n), gtotaldl/tc);
	}
	
	
	while (gexit==0) {sleep(1);}
	xs_logger_fatal ("...done...");
	
	if (u) u=xs_uri_destroy(u);


#else
  struct mg_callbacks callbacks;
  struct mg_context *ctx;

  //init rwlock
  pthread_rwlock_init(&rwlock, 0);

  // Initialize random number generator. It will be used later on for
  // the session identifier creation.
  srand((unsigned) time(0));

  // Setup and start Mongoose
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.begin_request = begin_request_handler;
  callbacks.websocket_ready = websocket_ready_handler;
  callbacks.websocket_data = websocket_data_handler;
  if ((ctx = mg_start(&callbacks, NULL, options)) == NULL) {
    printf("%s\n", "Cannot start chat server, fatal exit");
    myexit(EXIT_FAILURE);
  }

  // Wait until enter is pressed, then exit
  printf("Chat server started on ports %s, press enter to quit.\n",
         mg_get_option(ctx, "listening_ports"));
  getchar();
  mg_stop(ctx);
  printf("%s\n", "Chat server stopped.");

  pthread_rwlock_destroy (&rwlock);
#endif
  return EXIT_SUCCESS;
}

// vim:ts=2:sw=2:et

// Mark required libraries
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif

#define _xs_IMPLEMENTATION_
#include "xs/xs_socket.h"
#include "xs/xs_connection.h"
#include "xs/xs_ssl.h"
#include "xs/xs_queue.h"
#include "xs/xs_crc.h"
#include "xs/xs_sha1.h"
#include "xs/xs_compress.h"
#include "xs/xs_logger.h"
#include "xs/xs_fileinfo.h"
#include "xs/xs_server.h"
#include "xs/xs_startup.h"
#undef _xs_IMPLEMENTATION_

#define CHUNK   1024


#if 0
/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int inf(FILE *source)
{
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];
    FILE *dest=stdout;

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit2(&strm, -15);
    if (ret != Z_OK)
        return ret;

    fread(in,1,16,source);//skip header;
    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                return ret;
            }
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}
#endif


int dodecompress(char* path) {
    int ret=0, count, wtot=0;
    char in[CHUNK];
    char out[CHUNK];
    FILE *f, *fo;
    xs_compress *c;

    fo=stdout;
    //fo=fopen("b.out", "wb");
    if (fo==0) return -2;
#ifdef WIN32
    if (fo==stdout) setmode(fileno(stdout), O_BINARY);
#endif
    f = fopen(path, "rb");
    if (f==0) return -1;

    //return inf(f);

    xs_compress_create(&c, exs_Decompress_GZIP, -1);

    do {
        do {
            count = xs_compress_out(c, out, sizeof(out));
            ret = xs_compress_err(c);   
            if (count<0) {ret=count; break;}
            fwrite (out, 1, count, fo);
            wtot+=count;
        } while (count>0 && ret==0);
        
        if (ret) break;

        count = fread(in, 1, CHUNK, f);
        if (ferror(f) || count==0) {
            ret = 0;
            break;
        }

        xs_compress_in (c, in, count, feof(f));
    } while (ret==0);

    // clean up and return
    c = xs_compress_destroy(c);
    fclose (f);
    if (fo!=stdout) fclose(fo);
    return ret;
}

int docompress(char *path) {
    int ret=0, count, rtot=0;
	char in[CHUNK];
	char out[CHUNK];
    FILE *f, *fo;
    xs_compress *c;

    fo=stdout;
    //fo=fopen("b.out.gz", "wb");
    if (fo==0) return -2;
#ifdef WIN32
    if (fo==stdout) setmode(fileno(stdout), O_BINARY);
#endif  
    f = fopen(path, "rb");
    if (f==0) return -1;

    xs_compress_create(&c, exs_Compress_GZIP, -9);

    do {
       do {
            count = xs_compress_out(c, out, sizeof(out));
            ret = xs_compress_err(c);   
            fwrite (out, 1, count, fo);
        } while (count>0 && ret==0);
        
        if (ret) break;

        count = fread(in, 1, CHUNK, f);
        if (ferror(f) || count==0) {
            ret = 0;
            break;
        }
        rtot += count;
        xs_compress_in (c, in, count, feof(f)!=0);

    } while (ret==0);

    // clean up and return
    c = xs_compress_destroy(c);
    fclose (f);
    if (fo!=stdout) fclose (fo);
    return 0;
}


#include "xs/tinysvcmdns/mdnsd.h"

int bcast_cb (struct xs_async_connect* xas, int message, xs_conn* conn) {
    char buf[8192];
    size_t len;
    int reread;
    (void)xas;
    if (message==exs_Conn_Read) {
        if (1) do {
	        len = xs_conn_read(conn, buf, sizeof(buf)-1, &reread);//read (xs_conn_getsock(conn), buf, sizeof(buf-1));
            if (len<8192) {
                buf[len]=0;
                printf ("%s", buf);
            }
        } while (len>0);
        printf ("--------read end\n");
    }
    return 0;
}

int do_mdns() {
    struct mdnsd* d = mdnsd_start();
	const char *txt[] = {
		"path=/bfsree_server", 
		NULL
	};


    mdnsd_set_hostname (d, "webxs.local", inet_addr("10.0.0.145"));
    mdnsd_register_svc (d, "myinstance", "_http._tcp.local", 8080, 0, txt);
    return 0;
}

int broadcastmdns (void) {

#if 1
    /*
    M-SEARCH * HTTP/1.1
HOST: 239.255.255.250:1900
MAN: ssdp:discover
MX: 10
ST: ssdp:all*/
    
    struct in_addr localInterface;
    struct sockaddr_in groupSock;
    int sd;
    char databuf[] = 
                    "M-SEARCH * HTTP/1.1\r\n"
                    "HOST: 239.255.255.250:1900\r\n"
                    "MAN: \"ssdp:discover\"\r\n"
                    "MX: 3\r\n"
                    "ST: ssdp:all\r\n"
                    "USER-AGENT: mine\r\n"
                    "\r\n";
    char readbuf[8192];

    int datalen = strlen(databuf);
    int s, err, reuse=1;
    struct sockaddr_in localSock;
    struct ip_mreq group;
    xs_conn* mc;
    xs_async_connect* xas=0;

#if 1
    do_mdns();
#endif

#if 1
    s = socket(AF_INET, SOCK_DGRAM, 0);//(xs_sock_open (&s, "239.255.255.250", 1900, 0);
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
    /* Bind to the proper port number with the IP address */
    /* specified as INADDR_ANY. */
    memset((char *) &localSock, 0, sizeof(localSock));
    localSock.sin_family = AF_INET;
    localSock.sin_port = htons(19000);
    localSock.sin_addr.s_addr = INADDR_ANY;
    if((err=bind(s, (struct sockaddr*)&localSock, sizeof(localSock))))
    {
        perror("Binding datagram socket error");
        close(s);
        myexit(1);
    }
    else
        printf("Binding datagram socket...OK.\n");

     err = xs_conn_opensock (&mc, s, 0);

    /* Join the multicast group 226.1.1.1 on the local 203.106.93.94 */
    /* interface. Note that this IP_ADD_MEMBERSHIP option must be */
    /* called for each local interface over which the multicast */
    /* datagrams are to be received. */
    group.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
#if WIN32
    group.imr_interface.s_addr = inet_addr("10.0.0.145");
#else
    group.imr_interface.s_addr = INADDR_ANY;//inet_addr("10.0.2.255");
#endif
    if(setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
    {
        perror("Adding multicast group error");
        close(s);
        myexit(1);
    }
    else
        printf("Adding multicast group...OK.\n");
    if(setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, (char *)&group, sizeof(group)) < 0)
    {
        perror("Setting local interface error");
        myexit(1);
    }
    else
        printf("Setting the local interface...OK\n"); 
    if (1) xas = xs_async_read (0, mc, bcast_cb);
    //xs_conn_close(mc);
    //return 0;
    
    memset((char *) &groupSock, 0, sizeof(groupSock));
    groupSock.sin_family = AF_INET;
    groupSock.sin_addr.s_addr = inet_addr("239.255.255.250");
    groupSock.sin_port = htons(1900);
    if(sendto(s, databuf, datalen, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0)
        {perror("Sending datagram message error");}
    else
        printf("Sending datagram message...OK\n");
        
    return 0;
    
#endif   

    /* Create a datagram socket on which to send. */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd < 0)
    {
        perror("Opening datagram socket error");
        myexit(1);
    }
    else
        printf("Opening the datagram socket...OK.\n");

    /* Initialize the group sockaddr structure with a */
    /* group address of 225.1.1.1 and port 5555. */
    memset((char *) &groupSock, 0, sizeof(groupSock));
    groupSock.sin_family = AF_INET;
    groupSock.sin_addr.s_addr = inet_addr("239.255.255.250");
    groupSock.sin_port = htons(1900);

    /* Disable loopback so you do not receive your own datagrams.
    {
    char loopch = 0;
    if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0)
    {
    perror("Setting IP_MULTICAST_LOOP error");
    close(sd);
    myexit(1);
    }
    else
    printf("Disabling the loopback...OK.\n");
    }
    */

    /* Set local interface for outbound multicast datagrams. */
    /* The IP address specified must be associated with a local, */
    /* multicast capable interface. */
    localInterface.s_addr = inet_addr("10.0.0.145");
    if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0)
    {
        perror("Setting local interface error");
        myexit(1);
    }
    else
        printf("Setting the local interface...OK\n");
    
    err = xs_conn_opensock (&mc, sd, 0);
    xs_async_read (xas, mc, bcast_cb);

    /* Send a message to the multicast group specified by the*/
    /* groupSock sockaddr structure. */
    /*int datalen = 1024;*/
    if(sendto(sd, databuf, datalen, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0)
        {perror("Sending datagram message error");}
    else
        printf("Sending datagram message...OK\n");
    
    if (0) {
    if((datalen=recv(sd, readbuf, sizeof(readbuf), 0)) < 0)
    {
        perror("Reading datagram message error\n");
        close(sd);
        myexit(1);
    }
    else
    {
        printf("Reading datagram message from client...OK\n");
        printf("The message is: %s\n", databuf);
    }
    }

    //xs_sock_close (sd);

#endif
    return 0;
}   



#include "xs/tinysvcmdns/mdnsd.c"
#include "xs/tinysvcmdns/mdns.c"
