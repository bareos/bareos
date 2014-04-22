/* unit test the code in droplet.c */
#define _GNU_SOURCE	/* for RTLD_NEXT */
#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef __linux__
#include <pwd.h>
#include <dlfcn.h>
#endif
#include <errno.h>
#include <check.h>
#include <droplet.h>
#include <droplet/backend.h>

#include "utest_main.h"

/*
 * Note that because the libcheck framework runs each test in it's
 * process, we can get away with futzing with our own environment and
 * not cleaning up afterwards.
 *
 * Note we can't just change the $HOME environment variable because
 * Droplet doesn't look at it, instead it does a getpwuid(getuid()) to
 * work out the home directory.  So we have to do some horrible
 * juggling to mock getpwuid().  That juggling currently uses some
 * Linux-specific runtime linker voodoo, so we only build these
 * tests on Linux.
 *
 * random words courtesy http://hipsteripsum.me/
 */

#ifdef __linux__

/* really dumb and inefficient but simple strconcat() */
static char *
strconcat(const char *s, ...)
{
  int len = 0;	  /* not including trailing NUL */
  int i;
#define MAX_STRS  32
  int nstrs = 0;
  const char *strs[MAX_STRS];
  char *res;
  va_list args;

  va_start(args, s);
  while (NULL != s)
    {
      fail_unless(nstrs < MAX_STRS);
      strs[nstrs++] = s;
      len += strlen(s);
      s = va_arg(args, const char*);
    }
  va_end(args);

  res = malloc(len+1);
  dpl_assert_ptr_not_null(res);
  res[0] = '\0';

  for (i = 0 ; i < nstrs ; i++)
    strcat(res, strs[i]);

  return res;
#undef MAX_STRS
}

static char *
make_unique_directory(void)
{
  char *path;
  char cwd[PATH_MAX];
  char unique[64];
  static int idx;

  dpl_assert_ptr_not_null(getcwd(cwd, sizeof(cwd)));
  snprintf(unique, sizeof(unique), "%d.%d", (int)getpid(), idx++);
  path = strconcat(cwd, "/tmp.utest.", unique, (char*)NULL);

  if (mkdir(path, 0755) < 0)
    {
      perror(path);
      fail();
    }

  return path;
}

static char *
make_sub_directory(const char *parent, const char *child)
{
  char *path = strconcat(parent, "/", child, (char *)NULL);

  if (mkdir(path, 0755) < 0)
    {
      perror(path);
      fail();
    }

  return path;
}

static void
rmtree(const char *path)
{
  char cmd[PATH_MAX];

  snprintf(cmd, sizeof(cmd), "/bin/rm -rf '%s'", path);
  system(cmd);
}

static char *
write_file(const char *parent, const char *child, const char *contents)
{
  char *path = strconcat(parent, "/", child, (char *)NULL);
  int fd;
  int remain;
  int n;

  fd = open(path, O_WRONLY|O_TRUNC|O_CREAT, 0666);
  if (fd < 0)
    {
      perror(path);
      fail();
    }

  remain = strlen(contents);
  while (remain > 0)
    {
      n = write(fd, contents, strlen(contents));
      if (n < 0)
	{
	  if (errno == EINTR)
	    continue;
	  perror(path);
	  fail();
	}
      remain -= n;
      contents += n;
    }

  close(fd);

  return path;
}

static char *home = NULL;

/* mocked version of getpwuid(). */

struct passwd *
getpwuid(uid_t uid)
{
  static struct passwd *(*real_getpwuid)(uid_t) = NULL;
  struct passwd *pwd = NULL;

  if (real_getpwuid == NULL)
    {
      real_getpwuid = (struct passwd *(*)(uid_t))dlsym(RTLD_NEXT, "getpwuid");
    }
  pwd = real_getpwuid(uid);
  if (home && pwd)
    pwd->pw_dir = home;
  return pwd;
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
  dpl_free();

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
  dpl_assert_int_eq(ctx->use_https, 1);
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
  dpl_assert_int_eq(ctx->encode_slashes, 1);

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
  suite_add_tcase(s, t);
  return s;
}

#endif /* __linux__ */
