/* unit test the code in droplet.c */
#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <check.h>
#include <droplet.h>
#include <droplet/backend.h>
#include <arpa/inet.h>
#include <errno.h>

#include "testutils.h"
#include "utest_main.h"

#ifdef __linux__

static int bound_sock = -1;

static int
get_bound_port(char *host, int maxlen)
{
    struct sockaddr_in sin;
    socklen_t len;
    int sock;
    int r;

    /* only one at a time */
    fail_if(bound_sock >= 0, NULL);

    /* create a TCP socket */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    fail_if(sock < 0, strerror(errno));

    /* bind the socket to some port on localhost */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = 0;	/* kernel allocates */
    r = bind(sock, (struct sockaddr *)&sin, sizeof(sin));
    fail_if(r < 0, strerror(errno));

    /* note: we never listen() on the socket, so attempts
     * to connect() should fail immediately with ECONNREFUSED */

    /* find out what port the kernel chose for us */
    len = sizeof(sin);
    r = getsockname(sock, (struct sockaddr *)&sin, &len);
    fail_if(r < 0, strerror(errno));

    bound_sock = sock;

    snprintf(host, maxlen, "%s:%d", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
    return 0;
}

static void
release_bound_port(void)
{
    if (bound_sock >= 0)
      {
	close(bound_sock);
	bound_sock = -1;
      }
}

static void
setup(void)
{
  /* Make sure there's a new unique directory which
   * is returned as our home directory by getpwuid() */
  home = make_unique_directory();

  dpl_init();
}

static void
teardown(void)
{
  release_bound_port();

  rmtree(home);
  free(home);
  home = NULL;
}

START_TEST(ctx_new_defaults_test)
{
  dpl_ctx_t *ctx;
  char *dropdir;
  char *profile;

  /* Make sure there's a directory ~/.droplet
   * containing a file default.profile */
  dropdir = make_sub_directory(home, ".droplet");
  profile = write_file(dropdir, "default.profile",
    "host = localhost\n"
    "base_path = /polaroid\n");

  /* create a context with all defaults */
  unsetenv("DPLDIR");
  unsetenv("DPLPROFILE");
  ctx = dpl_ctx_new(NULL, NULL);
  dpl_assert_ptr_not_null(ctx);
  dpl_assert_str_eq(ctx->droplet_dir, dropdir);
  dpl_assert_str_eq(ctx->profile_name, "default");
  dpl_assert_str_eq(ctx->base_path, "/polaroid");

  dpl_ctx_free(ctx);
  free(dropdir);
  free(profile);
}
END_TEST

START_TEST(ctx_new_dropdir_test)
{
  dpl_ctx_t *ctx;
  char *dropdir;
  char *profile;

  /* Make sure there's a new unique directory,
   * which is *NOT* ~/.droplet, and contains a
   * file default.profile
   * random word courtesy http://hipsteripsum.me/ */
  dropdir = make_sub_directory(home, "williamsburg");
  profile = write_file(dropdir, "default.profile",
    "host = localhost\n"
    "base_path = /polaroid\n");

  /* create a context with all droplet dir named via the environment */
  setenv("DPLDIR", dropdir, /*overwrite*/1);
  unsetenv("DPLPROFILE");
  ctx = dpl_ctx_new(NULL, NULL);
  dpl_assert_ptr_not_null(ctx);
  dpl_assert_str_eq(ctx->droplet_dir, dropdir);
  dpl_assert_str_eq(ctx->profile_name, "default");
  dpl_assert_str_eq(ctx->base_path, "/polaroid");

  dpl_ctx_free(ctx);
  free(dropdir);
  free(profile);
}
END_TEST

START_TEST(ctx_new_dropdir_profile_test)
{
  dpl_ctx_t *ctx;
  char *dropdir;
  char *profile;

  /* Make sure there's a new unique directory,
   * which is *NOT* ~/.droplet, and contains a
   * file profile *NOT* called default.profile. */
  dropdir = make_sub_directory(home, "quinoa");
  profile = write_file(dropdir, "sriracha.profile",
    "host = localhost\n"
    "base_path = /polaroid\n");

  /* create a context with profile named via the environment */
  setenv("DPLDIR", dropdir, /*overwrite*/1);
  setenv("DPLPROFILE", "sriracha", /*overwrite*/1);
  ctx = dpl_ctx_new(NULL, NULL);
  dpl_assert_ptr_not_null(ctx);
  dpl_assert_str_eq(ctx->droplet_dir, dropdir);
  dpl_assert_str_eq(ctx->profile_name, "sriracha");
  dpl_assert_str_eq(ctx->base_path, "/polaroid");

  dpl_ctx_free(ctx);
  free(dropdir);
  free(profile);
}
END_TEST

START_TEST(ctx_new_from_dict_test)
{
  dpl_ctx_t *ctx;
  dpl_dict_t *profile;
  char *dropdir;

  /* At this point we have a pretend ~ with NO
   * .droplet/ directory in it.  We don't create
   * a dropdir on disk at all, nor a profile, it's
   * all in memory. */
  dropdir = strconcat(home, "/trust-fund", (char *)NULL);  /* never created */

  profile = dpl_dict_new(13);
  dpl_assert_ptr_not_null(profile);
  dpl_assert_int_eq(DPL_SUCCESS, dpl_dict_add(profile, "host", "localhost", 0));
  dpl_assert_int_eq(DPL_SUCCESS, dpl_dict_add(profile, "base_path", "/polaroid", 0));
  dpl_assert_int_eq(DPL_SUCCESS, dpl_dict_add(profile, "droplet_dir", dropdir, 0));
  dpl_assert_int_eq(DPL_SUCCESS, dpl_dict_add(profile, "profile_name", "viral", 0));
  /* need this to disable the event log, otherwise the droplet_dir needs to exist */
  dpl_assert_int_eq(DPL_SUCCESS, dpl_dict_add(profile, "pricing_dir", "", 0));

  /* create a context with all defaults */
  unsetenv("DPLDIR");
  unsetenv("DPLPROFILE");
  ctx = dpl_ctx_new_from_dict(profile);
  dpl_assert_ptr_not_null(ctx);
  dpl_assert_str_eq(ctx->droplet_dir, dropdir);
  dpl_assert_str_eq(ctx->profile_name, "viral");
  dpl_assert_str_eq(ctx->base_path, "/polaroid");

  dpl_ctx_free(ctx);
  dpl_dict_free(profile);
  free(dropdir);
}
END_TEST

/* actually set some non-default parameters to check they're parsed */
START_TEST(ctx_new_params_test)
{
  dpl_ctx_t *ctx;
  char *dropdir;
  char *profile;
  char *ssl_cert_file;
  char *ssl_key_file;
  char *ssl_ca_cert_file;
  char *profdata;
  char *pricing;
  static const char ssl_cert[] =
  /* This is actually a chain, starting with the client cert
   * and ending with the CA cert, in that order */
#include "ssldata/client-cert.c"
#include "ssldata/demoCA/cacert.c"
    ;
  static const char ssl_key[] =
#include "ssldata/client.c"
    ;
  static const char ssl_ca_cert[] =
#include "ssldata/demoCA/cacert.c"
    ;

  /* Make sure there's a directory ~/.droplet
   * containing a file default.profile */
  dropdir = make_sub_directory(home, ".droplet");
  ssl_cert_file = write_file(dropdir, "disrupt.pem", ssl_cert);
  ssl_key_file = write_file(dropdir, "fingerstache.pem", ssl_key);
  ssl_ca_cert_file = write_file(home, "stumptown.pem", ssl_ca_cert);
  pricing = write_file(dropdir, "pitchfork.pricing",
		       "# this file is empty\n");
  profdata = strconcat(
    "use_https = true\n"
    "host = localhost\n"
    "blacklist_expiretime = 42\n"
    "header_size = 12345\n"
    "base_path = /polaroid\n"
    "access_key = letterpress\n"
    "secret_key = freegan\n"
    "ssl_cert_file = disrupt.pem\n"
    "ssl_key_file = fingerstache.pem\n"
    "ssl_password = mixtape\n"
    "ssl_ca_list = ", home, "/stumptown.pem\n"
    "pricing = pitchfork\n"
    "read_buf_size = 8765\n"
    "encrypt_key = distillery\n"
    "backend = swift\n"
    "encode_slashes = true\n",
    (char *)NULL);
  profile = write_file(dropdir, "default.profile", profdata);

  /* create a context with all defaults */
  unsetenv("DPLDIR");
  unsetenv("DPLPROFILE");
  ctx = dpl_ctx_new(NULL, NULL);
  dpl_assert_ptr_not_null(ctx);
  dpl_assert_str_eq(ctx->droplet_dir, dropdir);
  dpl_assert_str_eq(ctx->profile_name, "default");
  dpl_assert_str_eq(ctx->base_path, "/polaroid");
  dpl_assert_bit_eq(ctx->use_https, 1);
  dpl_assert_int_eq(ctx->blacklist_expiretime, 42);
  dpl_assert_int_eq(ctx->header_size, 12345);
  dpl_assert_str_eq(ctx->access_key, "letterpress");
  dpl_assert_str_eq(ctx->secret_key, "freegan");
  dpl_assert_str_eq(ctx->ssl_cert_file, ssl_cert_file);
  dpl_assert_str_eq(ctx->ssl_key_file, ssl_key_file);
  dpl_assert_str_eq(ctx->ssl_password, "mixtape");
  dpl_assert_str_eq(ctx->ssl_ca_list, ssl_ca_cert_file);
  dpl_assert_str_eq(ctx->pricing, "pitchfork");
  dpl_assert_ptr_null(ctx->pricing_dir);
  dpl_assert_int_eq(ctx->read_buf_size, 8765);
  dpl_assert_str_eq(ctx->encrypt_key, "distillery");
  dpl_assert_str_eq(ctx->backend->name, "swift");
  dpl_assert_bit_eq(ctx->encode_slashes, 1);

  dpl_ctx_free(ctx);
  free(dropdir);
  free(profile);
  free(ssl_cert_file);
  free(ssl_key_file);
  free(ssl_ca_cert_file);
  free(pricing);
  free(profdata);
}
END_TEST

static void
redirect_stdio(FILE *fp, const char *name)
{
  char *logfile;
  int fd;

  logfile = strconcat(home, "/", name, ".log", (char *)NULL);
  fd = open(logfile, O_WRONLY|O_CREAT|O_TRUNC, 0666);
  if (fd < 0)
    {
      perror(logfile);
      fail();
    }
  fflush(fp);
  if (dup2(fd, fileno(fp)) < 0)
    {
      perror("dup2");
      fail();
    }
  close(fd);
  free(logfile);
}

#define MAXLOGGED 128
static struct {
  dpl_log_level_t level;
  char *message;
} logged[MAXLOGGED];
static int nlogged = 0;

static int
log_find(const char *needle)
{
  int i;

  for (i = 0 ; i < nlogged ; i++)
    {
      if (strstr(logged[i].message, needle))
	return i;
    }

  return -1;
}

static void
log_func(dpl_ctx_t *ctx, dpl_log_level_t level, const char *message)
{
  fail_unless(nlogged < MAXLOGGED);
  logged[nlogged].level = level;
  logged[nlogged].message = strdup(message);
  nlogged++;
}

START_TEST(logging_test)
{
  dpl_ctx_t *ctx;
  dpl_dict_t *profile;
  char *dropdir;
  off_t logsize;
  int nerrs;

  /* because libcheck forks a new process for each testcase, we
   * can feel free to futz around with the process state like stderr */
  redirect_stdio(stdout, "stdout");
  redirect_stdio(stderr, "stderr");

  /* At this point we have a pretend ~ with NO
   * .droplet/ directory in it.  We don't create
   * a dropdir on disk at all, nor a profile, it's
   * all in memory. */
  dropdir = strconcat(home, "/trust-fund", (char *)NULL);  /* never created */

  profile = dpl_dict_new(13);
  dpl_assert_ptr_not_null(profile);
  /* the host is an unparseable IPv4 address literal, which should
   * result in a predicable error */
  dpl_assert_int_eq(DPL_SUCCESS, dpl_dict_add(profile, "host", "123.456.789.012", 0));
  dpl_assert_int_eq(DPL_SUCCESS, dpl_dict_add(profile, "droplet_dir", dropdir, 0));
  dpl_assert_int_eq(DPL_SUCCESS, dpl_dict_add(profile, "profile_name", "viral", 0));
  /* need this to disable the event log, otherwise the droplet_dir needs to exist */
  dpl_assert_int_eq(DPL_SUCCESS, dpl_dict_add(profile, "pricing_dir", "", 0));
  unsetenv("DPLDIR");
  unsetenv("DPLPROFILE");

  /* Create a context which will fail with a logged error message.
   * By default error messages go to stderr only. */
  dpl_assert_int_eq(0, file_size(stdout));
  dpl_assert_int_eq(0, file_size(stderr));
  ctx = dpl_ctx_new_from_dict(profile);
  dpl_assert_ptr_null(ctx);
  /* nothing went to stdout */
  dpl_assert_int_eq(0, file_size(stdout));
  /* something went to stderr */
  logsize = file_size(stderr);
  fail_unless(logsize > 0);

  /* Create a context which will fail with a logged error message.
   * Calling dpl_set_log_func redirects errors to the given function.  */
  dpl_assert_int_eq(nlogged, 0);
  dpl_set_log_func(log_func);
  ctx = dpl_ctx_new_from_dict(profile);
  dpl_assert_ptr_null(ctx);
  /* at least one error message logged */
  dpl_assert_int_ne(nlogged, 0);
  /* nothing went to stdout */
  dpl_assert_int_eq(0, file_size(stdout));
  /* nothing more went to stderr */
  dpl_assert_int_eq(logsize, file_size(stderr));
  /* errors mention the broken address */
  dpl_assert_int_ne(-1, log_find("123.456.789.012"));

  /* Create a context which will fail with a logged error message.
   * Calling dpl_set_log_func(NULL) redirects errors back to stderr. */
  nerrs = nlogged;
  dpl_set_log_func(NULL);
  ctx = dpl_ctx_new_from_dict(profile);
  dpl_assert_ptr_null(ctx);
  /* no more errors went to the log function */
  dpl_assert_int_eq(nerrs, nlogged);
  /* nothing went to stdout */
  dpl_assert_int_eq(0, file_size(stdout));
  /* some more went to stderr */
  fail_unless(file_size(stderr) > logsize);

  /* cleanup */
  dpl_dict_free(profile);
  free(dropdir);
}
END_TEST

/* Create a context talking to a valid address on which nothing
 * is listening.  This will fail during connect, and should log
 * a message. */
START_TEST(connect_logging_test)
{
  dpl_ctx_t *ctx;
  dpl_dict_t *profile;
  dpl_req_t *req;
  dpl_conn_t *conn = NULL;
  char *dropdir;
  int r;
  dpl_status_t s;
  char host[128];

  /* because libcheck forks a new process for each testcase, we
   * can feel free to futz around with the process state like stderr */
  redirect_stdio(stdout, "stdout");
  redirect_stdio(stderr, "stderr");

  /* At this point we have a pretend ~ with NO
   * .droplet/ directory in it.  We don't create
   * a dropdir on disk at all, nor a profile, it's
   * all in memory. */
  dropdir = strconcat(home, "/trust-fund", (char *)NULL);  /* never created */

  r = get_bound_port(host, sizeof(host));
  fail_if(r < 0, NULL);

  profile = dpl_dict_new(13);
  dpl_assert_ptr_not_null(profile);
  dpl_assert_int_eq(DPL_SUCCESS, dpl_dict_add(profile, "host", host, 0));
  dpl_assert_int_eq(DPL_SUCCESS, dpl_dict_add(profile, "droplet_dir", dropdir, 0));
  dpl_assert_int_eq(DPL_SUCCESS, dpl_dict_add(profile, "profile_name", "viral", 0));
  /* need this to disable the event log, otherwise the droplet_dir needs to exist */
  dpl_assert_int_eq(DPL_SUCCESS, dpl_dict_add(profile, "pricing_dir", "", 0));
  unsetenv("DPLDIR");
  unsetenv("DPLPROFILE");

  dpl_assert_int_eq(nlogged, 0);
  dpl_set_log_func(log_func);

  /* create the profile */
  ctx = dpl_ctx_new_from_dict(profile);
  dpl_assert_ptr_not_null(ctx);
  /* no errors so far */
  dpl_assert_int_eq(nlogged, 0);
  /* nothing went to stdout */
  dpl_assert_int_eq(0, file_size(stdout));
  /* nothing went to stderr */
  dpl_assert_int_eq(0, file_size(stderr));

  req = dpl_req_new(ctx);
  dpl_assert_ptr_not_null(req);
  req->behavior_flags &= ~DPL_BEHAVIOR_VIRTUAL_HOSTING;

  /* attempt to connect */
  s = dpl_try_connect(ctx, req, &conn);
  dpl_assert_int_eq(DPL_FAILURE, s);
  dpl_assert_ptr_null(conn);
  /* at least 1 error logged */
  dpl_assert_int_ne(nlogged, 0);
  /* an error mentions the broken address */
  dpl_assert_int_ne(-1, log_find(host));
  /* an error mentions the libc error message */
  dpl_assert_int_ne(-1, log_find(strerror(ECONNREFUSED)));
  /* nothing went to stdout */
  dpl_assert_int_eq(0, file_size(stdout));
  /* nothing went to stderr */
  dpl_assert_int_eq(0, file_size(stderr));

  dpl_req_free(req);
  dpl_ctx_free(ctx);
  dpl_dict_free(profile);
  release_bound_port();
  free(dropdir);
}
END_TEST


Suite *
profile_suite()
{
  Suite *s = suite_create("profile");
  TCase *t = tcase_create("base");
  tcase_set_timeout(t, 45);
  tcase_add_checked_fixture(t, setup, teardown);
  tcase_add_test(t, ctx_new_defaults_test);
  tcase_add_test(t, ctx_new_dropdir_test);
  tcase_add_test(t, ctx_new_dropdir_profile_test);
  tcase_add_test(t, ctx_new_from_dict_test);
  tcase_add_test(t, ctx_new_params_test);
  tcase_add_test(t, logging_test);
  tcase_add_test(t, connect_logging_test);
  suite_add_tcase(s, t);
  return s;
}

#endif /* __linux__ */
