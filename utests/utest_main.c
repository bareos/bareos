#include <stdlib.h>
#include <check.h>

#include "utest_main.h"


    int
main (int      argc,
      char  ** argv)
{
  SRunner   * r = srunner_create(dict_suite());
  srunner_add_suite(r, vec_suite());
  srunner_add_suite(r, sbuf_suite());
  srunner_add_suite(r, dbuf_suite());
  srunner_add_suite(r, ntinydb_suite());
  srunner_add_suite(r, taskpool_suite());
  srunner_run_all(r, CK_NORMAL);
  int number_failed = srunner_ntests_failed(r);
  srunner_free(r);
  return (0 == number_failed) ? EXIT_SUCCESS : EXIT_FAILURE;
}
