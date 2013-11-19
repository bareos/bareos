#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/stat.h>
#include <check.h>

#include "utest_main.h"

int verbose = 0;

/*
 * Arrange to valgrind ourself.  Normally this would be done outside in
 * the Makefile or a shell script by just running the executable under
 * valgrind, but using libtool and a local shared library makes this
 * pretty much impossible.  So we have to do it this way.
 */
#define VALGRIND_BIN	"/usr/bin/valgrind"
#define MAGIC_VAR "DONT_VALGRIND_YOURSELF_FOOL"

static void
be_valground(int argc, char **argv)
{
  struct stat sb;
  int r;
  char **newargv;
  int newargc = 0;

  /* Note: we don't use VALGRIND_IS_RUNNING() to avoid
   * depending on the valgrind package at build time */
  if (getenv(MAGIC_VAR))
    return;

  r = stat(VALGRIND_BIN, &sb);
  if (r < 0 || !S_ISREG(sb.st_mode))
    return;

  newargv = (char **)malloc((argc+7) * sizeof(char **));
  newargv[newargc++] = VALGRIND_BIN;
  newargv[newargc++] = "--tool=memcheck";
  newargv[newargc++] = "-q";
  newargv[newargc++] = "--leak-check=full";
  newargv[newargc++] = "--suppressions=valgrind.supp";
  newargv[newargc++] = "--num-callers=32";
  while (*argv)
    newargv[newargc++] = *argv++;
  newargv[newargc] = NULL;

  fprintf(stderr, "Running self under valgrind\n");
  putenv(MAGIC_VAR "=yes");

  execv(newargv[0], newargv);

  perror(VALGRIND_BIN);
  exit(1);
}

/*
 * How to run the test driver under a debugger.
 *
 * $ libtool --mode=execute gdb utests/.libs/alltests
 * (gdb) set args --debug
 * (gdb) break whichever_test
 * (gdb) run
 *
 */

int
main(int argc, char ** argv)
{
  int i;
  SRunner *r;
  int debug_flag = 0;
  enum print_output output = CK_NORMAL;
  enum { OPT_DEBUG=1, OPT_VERBOSE };
  static const struct option options[] = {
    { "--debug", no_argument, NULL, OPT_DEBUG },
    { "--verbose", no_argument, NULL, OPT_VERBOSE },
    { NULL, 0, NULL, 0 }
  };

  while (i = getopt_long(argc, argv, "", options, NULL) > 0)
    {
      switch (i)
	{
	case OPT_VERBOSE:
	  output = CK_VERBOSE;
	  verbose = 1;
	  break;
	case OPT_DEBUG:
	  debug_flag = 1;
	  break;
	default:
	  exit(1);
	}
    }

  if (debug_flag)
    putenv("CK_DEFAULT_TIMEOUT=1000000");
  else
    be_valground(argc, argv);

  r = srunner_create(dict_suite());
  srunner_add_suite(r, getdate_suite());
  srunner_add_suite(r, droplet_suite());
  srunner_add_suite(r, vec_suite());
  srunner_add_suite(r, sbuf_suite());
  srunner_add_suite(r, dbuf_suite());
  srunner_add_suite(r, ntinydb_suite());
  srunner_add_suite(r, taskpool_suite());
  srunner_add_suite(r, addrlist_suite());
  srunner_add_suite(r, util_suite());
  srunner_add_suite(r, sproxyd_suite());
  srunner_add_suite(r, utest_suite());
#ifdef __linux__
  srunner_add_suite(r, profile_suite());
#endif
  if (debug_flag)
    srunner_set_fork_status(r, CK_NOFORK);
  srunner_run_all(r, output);
  int number_failed = srunner_ntests_failed(r);
  srunner_free(r);
  return (0 == number_failed) ? EXIT_SUCCESS : EXIT_FAILURE;
}
