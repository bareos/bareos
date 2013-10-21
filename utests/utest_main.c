#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <check.h>

#include "utest_main.h"

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

  newargv = (char **)malloc((argc+5) * sizeof(char **));
  newargv[newargc++] = VALGRIND_BIN;
  newargv[newargc++] = "--tool=memcheck";
  newargv[newargc++] = "-q";
  newargv[newargc++] = "--leak-check=full";
  while (*argv)
    newargv[newargc++] = *argv++;
  newargv[newargc] = NULL;

  fprintf(stderr, "Running self under valgrind\n");
  putenv(MAGIC_VAR "=yes");

  execv(newargv[0], newargv);

  perror(VALGRIND_BIN);
  exit(1);
}

int
main(int argc, char ** argv)
{
  be_valground(argc, argv);
  SRunner   *r = srunner_create(dict_suite());
  srunner_add_suite(r, getdate_suite());
  srunner_add_suite(r, vec_suite());
  srunner_add_suite(r, sbuf_suite());
  srunner_add_suite(r, dbuf_suite());
  srunner_add_suite(r, ntinydb_suite());
  srunner_add_suite(r, taskpool_suite());
  srunner_add_suite(r, addrlist_suite());
  srunner_add_suite(r, util_suite());
  srunner_add_suite(r, utest_suite());
  srunner_run_all(r, CK_NORMAL);
  int number_failed = srunner_ntests_failed(r);
  srunner_free(r);
  return (0 == number_failed) ? EXIT_SUCCESS : EXIT_FAILURE;
}
