#include <stdio.h>
#include <droplet.h>
#include <droplet/dbuf.h>
#include "utest_main.h"

START_TEST(dbuf_test)
{
  dpl_dbuf_t    * b = dpl_dbuf_new();
  fail_if(NULL == b, NULL);
  ck_assert_int_eq(0, dpl_dbuf_length(b));
  dpl_dbuf_free(b);

  b = dpl_dbuf_new();
  fail_if(NULL == b, NULL);

  const char str[] = "abcdefghijklmnopqrstuvwxyz";
  int             ret;

  ret = dpl_dbuf_add(b, str, sizeof(str));
  ck_assert_int_eq(1, ret);
  ck_assert_int_eq(sizeof(str), dpl_dbuf_length(b));

  unsigned int  i;
  for (i = 0; i < sizeof(str); i++)
    {
      char  out;
      ret = dpl_dbuf_consume(b, &out, sizeof(out));
      ck_assert_int_eq(1, ret);
      ck_assert_int_eq(sizeof(str), dpl_dbuf_length(b) + i + 1);
      ck_assert_int_eq(out, str[i]);
    }
  dpl_dbuf_free(b);
}
END_TEST


Suite *
dbuf_suite()
{
  Suite *s = suite_create("dbuf");
  TCase *t = tcase_create("base");
  tcase_add_test(t, dbuf_test);
  suite_add_tcase(s, t);
  return s;
}
