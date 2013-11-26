#define _GNU_SOURCE	/* for RTLD_NEXT */
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <check.h>
#ifdef __linux__
#include <pwd.h>
#include <dlfcn.h>
#endif

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

/* really dumb and inefficient but simple strconcat() */
char *
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

char *
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

char *
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

void
rmtree(const char *path)
{
  char cmd[PATH_MAX];

  snprintf(cmd, sizeof(cmd), "/bin/rm -rf '%s'", path);
  system(cmd);
}

char *
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

off_t
file_size(FILE *fp)
{
  struct stat sb;

  fflush(fp);
  if (fstat(fileno(fp), &sb) < 0)
    {
      perror("fstat");
      fail();
    }
  return sb.st_size;
}


#ifdef __linux__

char *home = NULL;

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

#endif /* __linux__ */

