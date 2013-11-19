/* TODO: copyright */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <malloc.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <assert.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include "toyserver.h"

struct kv
{
  struct kv *next;
  char *key;
  char *value;
};

struct strmap
{
  const char *name;
  int value;
};

static int verbose = 0;
static struct server_state *state;
static struct kv *objects;

static void *
xmalloc(size_t sz)
{
  void *p = malloc(sz);
  assert(p != NULL);
  memset(p, 0, sz);
  return p;
}

static char *
xstrdup(const char *s)
{
  char *p;
  assert(s != NULL);
  p = strdup(s);
  assert(p != NULL);
  return p;
}

static void
kv_add(struct kv **listp, const char *key, const char *value)
{
  struct kv *p, **tailp;

  for (tailp = listp ; *tailp ; tailp = &(*tailp)->next)
    ;

  p = xmalloc(sizeof(struct kv));
  p->key = xstrdup(key);
  p->value = xstrdup(value);

  *tailp = p;
}

static void
kv_set(struct kv **listp, const char *key, const char *value)
{
  struct kv *p, **prevp;

  for (prevp = listp ; *prevp ; prevp = &(*prevp)->next)
    {
      p = *prevp;
      if (!strcasecmp(p->key, key))
	{
	  free(p->value);
	  p->value = strdup(value);
	  return;
	}
    }

  p = xmalloc(sizeof(struct kv));
  p->key = xstrdup(key);
  p->value = xstrdup(value);

  *prevp = p;
}

static void
kv_free(struct kv **listp)
{
  while (*listp)
    {
      struct kv *p = *listp;
      *listp = (*listp)->next;
      free(p->key);
      free(p->value);
      free(p);
    }
}

static const char *
kv_ifind(struct kv **listp, const char *key)
{
  struct kv **pp;

  for (pp = listp ; *pp ; pp = &(*pp)->next)
    {
      if (!strcasecmp(key, (*pp)->key))
	return (*pp)->value;
    }
  return NULL;
}

/*
 * Create and map the statefile.  This is mapped shared so that
 * the clients can open and mmap it to check the server status
 * and control the server behaviour.  We write-lock the file
 * to avoid two servers starting up on the same statefile;
 * clients do not lock the statefile.  Sets up the global
 * `state' pointer.
 */
static int
setup_state(const char *statefile)
{
  int fd;
  int n;
  void *p;
  struct flock flock;
  struct stat sb;
  struct server_state dummy;

  if (verbose)
    fprintf(stderr, "Creating statefile \"%s\"\n", statefile);

  /*
   * Open the file.
   */
  fd = open(statefile, O_RDWR|O_CREAT, 0666);
  if (fd < 0)
    {
      perror(statefile);
      return -1;
    }

  /*
   * Request a write lock on the whole file.
   */
  memset(&flock, 0, sizeof(flock));
  flock.l_type = F_WRLCK;
  flock.l_whence = SEEK_SET;
  flock.l_start = 0;
  flock.l_len = 0;
  n = fcntl(fd, F_SETLK, &flock);
  if (n < 0)
    {
      if (errno == EAGAIN || errno == EACCES)
	{
	  /* a conflicting lock */
	  fcntl(fd, F_GETLK, &flock);
	  if (flock.l_pid)
	    fprintf(stderr, "%s: already locked by server process pid %d\n",
		    statefile, flock.l_pid);
	  else
	    fprintf(stderr, "%s: already locked by another server process\n",
		    statefile);
	}
      else
	{
	  /* something else went wrong */
	  perror(statefile);
	}
      close(fd);
      return -1;
    }

  if (verbose)
    fprintf(stderr, "Statefile locked, truncating and zeroing\n");

  /*
   * Now that we now we own the file, make it the right
   * size and fill it with zeros.
   */
  n = ftruncate(fd, 0);
  if (n < 0)
    {
      perror(statefile);
      close(fd);
      return -1;
    }
  memset(&dummy, 0, sizeof(dummy));
  n = write(fd, &dummy, sizeof(dummy));
  if (n < 0)
    {
      perror(statefile);
      close(fd);
      return -1;
    }
  if (n < sizeof(dummy))
    {
      fprintf(stderr, "%s: short write\n", statefile);
      close(fd);
      return -1;
    }

  if (verbose)
    fprintf(stderr, "Mapping statefile\n");

  p = mmap(NULL, sizeof(dummy), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED) {
      perror("mmap");
      close(fd);
      return -1;
  }

  /*
   * We keep the file descriptor open for the lifetime of the
   * server process, which keeps the file lock open too.
   */

  state = (struct server_state *)p;
  return 0;
}

/*
 * Create a bound and listening TCP4 server socket, bound to the IPv4
 * loopback address and a port chosen by the kernel.
 *
 * Returns: socket, or -1 on error.  *@portp is filled with the port
 * number.
 */
static int
create_server_socket(int *portp)
{
  int sock;
  struct sockaddr_in sin;
  int r;

  if (verbose)
    fprintf(stderr, "Creating rendezvous socket\n");

  sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0)
    {
      perror("socket(TCP)");
      return -1;
    }

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (*portp)
      sin.sin_port = htons(*portp);

  r = bind(sock, (struct sockaddr *)&sin, sizeof(sin));
  if (r < 0)
    {
      perror("bind");
      goto error;
    }

  r = listen(sock, 5);
  if (r < 0)
    {
      perror("listen");
      goto error;
    }

  if (!*portp)
    {
      socklen_t len = sizeof(sin);
      r = getsockname(sock, (struct sockaddr *)&sin, &len);
      if (r < 0) {
	  perror("getsockname");
	  goto error;
      }
      if (len != sizeof(sin) || sin.sin_family != AF_INET) {
	  fprintf(stderr, "Bad address from getsockname()\n");
	  goto error;
      }
      *portp = ntohs(sin.sin_port);
    }

    return sock;

error:
    close(sock);
    return -1;
}

/*
 * Block until an incoming connection is received, then establish the
 * connection and initialise per-connection state.  On success, set
 * @state->connection.is_connected.
 *
 * Returns 0 on success, -1 on failure (errors indicate some resource
 * limitation in the server, not any behaviour of the client, and
 * should be fatal).
 */
static int
server_accept(void)
{
  struct connection *conn = &state->connection;
  int conn_sock;
  int r;

  if (verbose)
    fprintf(stderr, "Server waiting for connection\n");

  conn_sock = accept(state->rend_sock, NULL, NULL);
  if (conn_sock < 0)
    {
      perror("accept");
      return -1;
    }
  if (verbose)
    fprintf(stderr, "Server accepted connection\n");

  conn->in = fdopen(conn_sock, "r");
  conn->out = fdopen(dup(conn_sock), "w");
  conn->is_authenticated = 0;
  conn->is_connected = 1;
  conn->is_persistent = 0;
  conn->is_tls = 0;
#if 0
  conn->tls_conn = NULL;
#endif

  return 0;
}

/*
 * Handle the end of a connection, by cleaning up all the per-connection
 * state.  In particular, clears @state->connection.is_connected.
 */
static void
server_unaccept(void)
{
  struct connection *conn = &state->connection;

  fclose(conn->in);
  conn->in = NULL;
  fclose(conn->out);
  conn->out = NULL;
  conn->is_connected = 0;
  conn->is_tls = 0;
#if 0
  if (conn->tls_conn)
    {
      tls_reset_servertls(&conn->tls_conn);
      conn->tls_conn = NULL;
    }
#endif
}

/*
 * Main routine for pushing text back to the client.
 */
static void server_printf(const char *fmt, ...)
    __attribute__((format(printf,1,2)));

static void
server_printf(const char *fmt, ...)
{
  va_list args;
  int r;
  char buf[2048];

  if (verbose)
    {
      va_start(args, fmt);
      fprintf(stderr, "S: ");
      vfprintf(stderr, fmt, args);
      va_end(args);
    }

  va_start(args, fmt);
  r = vsnprintf(buf, sizeof(buf), fmt, args);
  assert(r < (int)sizeof(buf));
  va_end(args);

  r = fwrite(buf, 1, strlen(buf), state->connection.out);
  assert(r >= 0);
}

/*
 * Flush any pending output back to the client.
 */
static void
server_flush(void)
{
  fflush(state->connection.out);
}

/*
 * Get a line of text from the client.  Blocks until
 * a while line is available.
 *
 * Returns: 0 on success, -1 on error or end of file.
 */
static int
server_getline(char *buf, int maxlen)
{
  if (!fgets(buf, maxlen, state->connection.in))
    return -1;
  if (verbose)
    fprintf(stderr, "C: %s", buf);
  return 0;
}

static void
trim_crlf(char *buf)
{
  char *p = strchr(buf, '\r');
  if (p)
    *p = '\0';
}

static char *
trim_whitespace(char *s)
{
  char *e;

  for ( ; *s && isspace(*s) ; s++)
    ;
  for (e = s + strlen(s)-1 ; e >= s && isspace(*e) ; e--)
    *e = '\0';
  return s;
}

static int
strmap_to_value(const struct strmap *map, const char *str)
{
  if (str == NULL)
    return -1;
  for ( ; map->name ; map++)
    {
      if (!strcasecmp(map->name, str))
	return map->value;
    }
  return -1;
}

static const char *
strmap_to_name(const struct strmap *map, int value)
{
  for ( ; map->name ; map++)
    {
      if (value == map->value)
	return map->name;
    }
  return NULL;
}

static const struct strmap version_strs[] =
  {
    { "HTTP/1.0", V_1_0 },
    { "HTTP/1.1", V_1_1 },
    { NULL, 0 }
  };
static const struct strmap method_strs[] =
  {
    { "OPTIONS", M_OPTIONS },
    { "GET", M_GET },
    { "HEAD", M_HEAD },
    { "POST", M_POST },
    { "PUT", M_PUT },
    { "DELETE", M_DELETE },
    { "TRACE", M_TRACE },
    { "CONNECT", M_CONNECT },
    { NULL, 0 }
  };
static const struct strmap status_strs[] =
  {
    { "Continue",				100 },
    { "Switching Protocols",			101 },
    { "OK",					200 },
    { "Created",				201 },
    { "Accepted",				202 },
    { "Non-Authoritative Information",		203 },
    { "No Content",				204 },
    { "Reset Content",				205 },
    { "Partial Content",			206 },
    { "Multiple Choices",			300 },
    { "Moved Permanently",			301 },
    { "Found",					302 },
    { "See Other",				303 },
    { "Not Modified",				304 },
    { "Use Proxy",				305 },
    { "Temporary Redirect",			307 },
    { "Bad Request",				400 },
    { "Unauthorized",				401 },
    { "Payment Required",			402 },
    { "Forbidden",				403 },
    { "Not Found",				404 },
    { "Method Not Allowed",			405 },
    { "Not Acceptable",				406 },
    { "Proxy Authentication Required",		407 },
    { "Request Time-out",			408 },
    { "Conflict",				409 },
    { "Gone",					410 },
    { "Length Required",			411 },
    { "Precondition Failed",			412 },
    { "Request Entity Too Large",		413 },
    { "Request-URI Too Large",			414 },
    { "Unsupported Media Type",			415 },
    { "Requested range not satisfiable",	416 },
    { "Expectation Failed",			417 },
    { "Internal Server Error",			500 },
    { "Not Implemented",			501 },
    { "Bad Gateway",				502 },
    { "Service Unavailable",			503 },
    { "Gateway Time-out",			504 },
    { "HTTP Version not supported",		505 },
    { NULL, 0 },
  };

static void
server_wait_for_request(void)
{
  int c = getc(state->connection.in);
  if (c != EOF)
    ungetc(c, state->connection.in);
}

static void
server_setup_request(void)
{
  struct request *req = &state->request;

  req->version = V_1_0;
  req->method = (enum http_methods)-1;
  kv_free(&req->headers);
  req->uri[0] = '\0';
  req->host[0] = '\0';
  req->body[0] = '\0';
  req->body_len = -1;
  req->username[0] = '\0';
  req->password[0] = '\0';
  req->has_authorization = 0;
}

/* Returns 0 on EOF, 200 on success, an HTTP status or -1 on error */
static int
server_read_request_start_line(void)
{
  struct request *req = &state->request;
  int r;
  char *p;
  char buf[1024];

  r = server_getline(buf, sizeof(buf));
  if (r < 0)
    return 0;	/* EOF */
  trim_crlf(buf);

  p = strtok(buf, " ");
  if (p == NULL)
    return 400; /* Bad Request */
  if (verbose)
    fprintf(stderr, "method=\"%s\"\n", p);
  req->method = strmap_to_value(method_strs, p);

  p = strtok(NULL, " ");
  if (p == NULL)
    return 400; /* Bad Request */
  strncpy(req->uri, p, sizeof(req->uri));
  req->uri[sizeof(req->uri)-1] = '\0';
  if (verbose)
    fprintf(stderr, "uri=\"%s\"\n", req->uri);

  p = strtok(NULL, " ");
  if (p == NULL)
    return 400; /* Bad Request */
  if (verbose)
    fprintf(stderr, "version=\"%s\"\n", p);
  req->version = strmap_to_value(version_strs, p);

  /* there should be no more fields */
  p = strtok(NULL, " ");
  if (p != NULL)
    return 400; /* Bad Request */

  /* Check the HTTP version */
  if (req->version < 0)
    {
      req->version = V_1_0; /* for emitting the error */
      return 505;	/* HTTP Version Not Supported */
    }

  /* Check the method */
  if (req->method < 0)
    return 400; /* Bad Request */

  if (verbose)
    fprintf(stderr, "Good request\n");
  return 200;
}

static int
server_set_request_authorization(char *str)
{
  struct request *req = &state->request;
  BIO *bio;
  char *p;
  int len;
  char buf[256];
  static const char sep[] = " \t\n\r";

  /* The Authorization: header is a type token naming the
   * scheme, here "Basic", followed by scheme specific data,
   * here base64(username:password) */
  p = strtok(str, sep);
  if (strcasecmp(p, "basic"))
    return 401;

  p = strtok(NULL, sep);
  if (!p)
    return 401;

  bio = BIO_new(BIO_f_base64());
  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
  bio = BIO_push(bio, BIO_new_mem_buf(p, -1));
  len = BIO_read(bio, buf, sizeof(buf));
  BIO_free_all(bio);
  if (len <= 0 || len >= sizeof(buf))
    return 401;
  buf[len] = '\0';

  p = strchr(buf, ':');
  if (!p)
    return 401;
  *p++ = '\0';

  strncpy(req->username, buf, sizeof(req->username));
  req->username[sizeof(req->username)-1] = '\0';

  strncpy(req->password, p, sizeof(req->password));
  req->password[sizeof(req->password)-1] = '\0';

  req->has_authorization = 1;
  if (verbose)
    fprintf(stderr, "Request has authorization header, "
	    "scheme=basic username=\"%s\" password=\"%s\"\n",
	    req->username, req->password);
  return 0;
}

static int
request_is_persistent(struct request *req)
{
  const char *p;

  if (req->version == V_1_0)
    return 0;	/* HTTP/1.0 */

  /* HTTP/1.1 */

  p = kv_ifind(&req->headers, "Connection");
  if (p && !strcasecmp(p, "close"))
    return 0;

  return 1;
}


/* Returns 0 on success, an HTTP status or -1 on error */
static int
server_read_request_headers(void)
{
  struct request *req = &state->request;
  int authorization_seen = 0;
  int r;
  char *p;
  char buf[1024];

  while (server_getline(buf, sizeof(buf)) == 0)
    {
      if (buf[0] == '\r' || buf[0] == '\n')
	break;   /* empty line is end of headers */

      /* We don't handle continuation headers */
      if (buf[0] == ' ' || buf[0] == '\t')
	{
	  if (verbose)
	    fprintf(stderr, "Found continuation header, failing\n");
	  return 400; /* Bad Request */
	}

      /* split the header name and value */
      p = strchr(buf, ':');
      if (p == NULL)
	return 400; /* Bad Request */
      *p++ = '\0';
      p = trim_whitespace(p);
      kv_add(&req->headers, buf, p);

      /* There are some header fields we
       * can parse as we read them */
      if (!strcasecmp(buf, "Host"))
	{
	  strncpy(req->host, p, sizeof(req->host));
	  req->host[sizeof(req->host)-1] = '\0';
	}
      else if (!strcasecmp(buf, "Content-Length"))
	{
	  req->body_len = atoi(p);
	}
      else if (!strcasecmp(buf, "Authorization"))
	{
	  if (authorization_seen++)
	    return 400;	/* Bad Request */
	  r = server_set_request_authorization(p);
	  if (r)
	    return r;
	}
    }

  /* end of file is end of headers too, if not very clean */

  if (verbose)
    {
      struct kv *kv;
      fprintf(stderr, "Request headers:\n");
      for (kv = req->headers ; kv ; kv = kv->next)
	fprintf(stderr, "    name=\"%s\" value=\"%s\"\n", kv->key, kv->value);
    }

  /* There are some header fields we can pick apart now */
  state->connection.is_persistent = request_is_persistent(req);

  return 0;
}

static int
server_read_request_body(void)
{
  struct request *req = &state->request;
  int remain;
  char *p;
  int r;

  if (!req->body_len < 0)
    {
      if (verbose)
	fprintf(stderr, "Cannot read request bodies without Content-Length\n");
      return -1;
    }
  if (req->body_len >= sizeof(req->body))
    {
      if (verbose)
	fprintf(stderr, "Request is just too large sorry\n");
      return -1;
    }

  remain = req->body_len;
  p = req->body;
  while (remain > 0)
    {
      r = fread(p, 1, remain, state->connection.in);
      if (r < 0)
	{
	  perror("read");
	  return -1;
	}
      if (r == 0)
	{
	  fprintf(stderr, "Short request\n");
	  return -1;
	}
      p += r;
      remain -= r;
    }

    if (verbose)
      fprintf(stderr, "Read %d bytes of request body\n", req->body_len);

    return 0;
}

/* Returns 0 on success, an HTTP status or -1 on error */
static int
server_handle_request(void)
{
  struct request *req = &state->request;
  struct reply *rep = &state->reply;
  struct connection *conn = &state->connection;
  const char *p;

  if (state->config.require_basic_auth)
    {
      /* enforce authorisation */
      if (!req->has_authorization)
	return 401;   /* Unauthorized */
      if (strcmp(req->username, TOY_USERNAME))
	return 403;   /* Forbidden */
      if (strcmp(req->password, TOY_PASSWORD))
	return 403;   /* Forbidden */
    }

  if (!strncmp(req->uri, "/proxy/chord/", 13))
    {
      const char *id = &req->uri[13];
      switch (req->method)
	{
	case M_PUT:
	  if (verbose)
	    fprintf(stderr, "Fake sproxyd PUT\n");
	  kv_set(&objects, id, req->body);
	  return 0;
	case M_GET:
	  if (verbose)
	    fprintf(stderr, "Fake sproxyd GET\n");
	  p = kv_ifind(&objects, id);
	  if (NULL == p)
	    return 404; /* Not Found */
	  strncpy(req->body, p, sizeof(req->body));
	  req->body[sizeof(req->body)-1] = '\0';
	  return 0;
	}
    }

  if (verbose)
    fprintf(stderr, "Faking a trivial reply for now\n");
  return 0;
}

static void
server_setup_reply(void)
{
  struct reply *rep = &state->reply;

  kv_free(&rep->headers);
  kv_add(&rep->headers, "Server", "toyserver/0.1");
  rep->status = 500;
  rep->body[0] = '\0';
  rep->body_len = 0;
}

static int
server_send_reply(void)
{
  struct reply *rep = &state->reply;
  int r;
  int remain;
  char *p;
  const char *ss;
  struct kv *kv;
  char lenbuf[32];

  if (verbose)
    fprintf(stderr, "Sending reply\n");

  snprintf(lenbuf, sizeof(lenbuf), "%d", rep->body_len);
  kv_add(&rep->headers, "Content-Length", lenbuf);

  /* Status line */
  ss = strmap_to_name(status_strs, rep->status);
  server_printf("%s %03d %s\r\n",
		strmap_to_name(version_strs, state->request.version),
		rep->status,
		ss ? ss : "Unknown status");

  if (rep->status == 401)
    kv_add(&rep->headers, "WWW-Authenticate", "Basic realm=\"WallyWorld\"");

  /* Reply headers */
  for (kv = rep->headers ; kv ; kv = kv->next)
    server_printf("%s: %s\r\n", kv->key, kv->value);
  server_printf("\r\n");

  /* Reply body */
  if (rep->body_len)
    {
      if (verbose)
	fprintf(stderr, "S: ...%d bytes...\n", rep->body_len);

      remain = rep->body_len;
      p = rep->body;
      while (remain > 0)
	{
	  r = fwrite(p, 1, remain, state->connection.out);
	  if (r < 0)
	    {
	      if (errno == EAGAIN)
		continue;
	      perror("write");
	      return -1;
	    }
	  p += r;
	  remain -= r;
	}
    }

  server_flush();
  return 0;
}

/*
* Server main request loop for a single connection.  Blocks waiting for
* requests from the client, and handles each request.
*
 * Initially only does HTTP/1.0 semantics of a single request per
 * connection.
 */
static void
server_request_loop(void)
{
  int r;

  do
    {
      /* Note - we carefully leave the last request's data
       * in place until the next request arrives, so that the
       * client has a chance to examine the server_state to
       * check that everything happened the way it expected */
      server_wait_for_request();

      server_setup_request();
      server_setup_reply();

      r = server_read_request_start_line();
      if (!r)
	return; /* EOF */
      if (r != 200)
	goto error_and_disconnect;

      r = server_read_request_headers();
      if (r)
	goto error_and_disconnect;

      r = server_read_request_body();
      if (r)
	goto error_and_disconnect;

      r = server_handle_request();
      if (r < 0)
	state->reply.status = 500;
      else if (r == 0 || r == 200)
	state->reply.status = 200;
      else
	state->reply.status = r;

      r = server_send_reply();
      if (r < 0)
	return;
    }
  while (state->connection.is_persistent);

  return;

error_and_disconnect:
  state->reply.status = (r < 0 ? 500 : r);
  server_send_reply();
}

static int
server_main_loop(void)
{
    /* Main connection loop.  This is a toy, single-threaded server
     * which handles one connection at a time. */
    for (;;)
      {
	/* Wait for a connection */
	if (server_accept() < 0)
	    exit(1);

	/* Run the main request loop for this connection */
	server_request_loop();

	/* Forget the per-connection state */
	server_unaccept();
      }
}

#define PIPE_READ 0
#define PIPE_WRITE 1

/*
 * Start the server.  Forks a child process, which does some once-off
 * server initialisation.  Returns 0 on success -1 on error.
 */
static int
server_setup(int foreground_flag, const char *statefile)
{
  int r;
  pid_t pid;
  int sync_pipe[2];
  int sock, port = 0;

  if (verbose)
      fprintf(stderr, "Initialising server!\n");

  if (!foreground_flag)
    {
      r = pipe(sync_pipe);
      if (r < 0)
	{
	  perror("pipe");
	  return -1;
	}

      pid = fork();
      if (pid < 0)
	{
	  perror("fork");
	  close(sync_pipe[PIPE_READ]);
	  close(sync_pipe[PIPE_WRITE]);
	  return -1;
	}
      if (pid)
	{
	  /* We are in the parent process.  */
	  char dummy;
	  close(sync_pipe[PIPE_WRITE]);
	  if (verbose)
	      fprintf(stderr, "Parent waiting for child to initialise\n");
	  r = read(sync_pipe[PIPE_READ], &dummy, 1);
	  if (r < 0)
	    {
	      perror("read(sync_pipe)");
	      exit(1);
	    }
	  if (r == 0)
	    {
	      if (verbose)
		fprintf(stderr, "Parent sees broken pipe, child died or errored\n");
	      exit(1);
	    }
	  if (verbose)
	      fprintf(stderr, "Parent sees child has finished initialising\n");
	  exit(0);
	}
      /* We are the child process. */
      close(sync_pipe[PIPE_READ]);
      if (verbose)
	fprintf(stderr, "Child pid is %d\n", (int)getpid());
    }

  if (setup_state(statefile) < 0)
    goto error;

  sock = create_server_socket(&port);
  if (sock < 0)
    goto error;

  state->port = port;
  state->rend_sock = sock;
  snprintf(state->url, sizeof(state->url), "http://localhost:%d/", port);
  state->pid = (int)getpid();

  if (verbose)
      fprintf(stderr, "Bound to port tcp/%u url is %s\n", port, state->url);

  if (!foreground_flag)
    {
      /* Tell the parent process we're done initialising */
      char dummy = 'X';
      if (verbose)
	  fprintf(stderr, "Child signalling parent\n");
      r = write(sync_pipe[PIPE_WRITE], &dummy, 1);
      if (r < 0)
	{
	  perror("write(sync_pipe)");
	  goto error;
	}
      close(sync_pipe[PIPE_WRITE]);
    }

  return 0;

error:
  /* in the child process */
  if (!foreground_flag)
    close(sync_pipe[PIPE_WRITE]);
  return -1;
}

int
main(int argc, char **argv)
{
  int i;
  char *statefile = "toyserver.state";
  int foreground_flag = 0;

  while ((i = getopt(argc, argv, "fs:v")) > 0)
    {
      switch (i)
	{
	case 'f':
	  foreground_flag = 1;
	  break;
	case 's':
	  statefile = optarg;
	  break;
	case 'v':
	  verbose++;
	  break;
	default:
	  fprintf(stderr, "%s: unknown option %c\n", argv[0], optopt);
	  exit(1);
	}
    }

  if (verbose)
    fprintf(stderr, "Toyserver starting\n");

  if (server_setup(foreground_flag, statefile) < 0)
    return 1;

  server_main_loop();

  return 0;
}

/* vim: set sw=2 sts=2: */
