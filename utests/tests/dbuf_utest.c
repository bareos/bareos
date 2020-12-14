#include <stdio.h>
#include <check.h>
#include <droplet.h>
#include <droplet/dbuf.h>

#include "utest_main.h"

START_TEST(dbuf_test)
{
  dpl_dbuf_t    * b = dpl_dbuf_new();
  dpl_assert_ptr_not_null(b);
  dpl_assert_int_eq(0, dpl_dbuf_length(b));
  dpl_dbuf_free(b);

  b = dpl_dbuf_new();
  dpl_assert_ptr_not_null(b);

  const char str[] = "abcdefghijklmnopqrstuvwxyz";
  int             ret;

  ret = dpl_dbuf_add(b, str, sizeof(str));
  dpl_assert_int_eq(1, ret);
  dpl_assert_int_eq(sizeof(str), dpl_dbuf_length(b));

#if 0
  unsigned int  i;
  for (i = 0; i < sizeof(str); i++)
    {
      char  out;
      ret = dpl_dbuf_consume(b, &out, sizeof(out));
      dpl_assert_int_eq(1, ret);
      dpl_assert_int_eq(sizeof(str), dpl_dbuf_length(b) + i + 1);
      dpl_assert_int_eq(out, str[i]);
    }
#endif
  dpl_dbuf_free(b);
}
END_TEST


#if 0
START_TEST(long_consume_test)
{
  dpl_dbuf_t *b = dpl_dbuf_new();
  int r;
  unsigned char cb[20];

  dpl_assert_int_eq(1, dpl_dbuf_add(b, "abcdefghij", 10));
  dpl_assert_int_eq(10, dpl_dbuf_length(b));

  /* try to consume more bytes than there are */
  memset(cb, 0xff, sizeof(cb));
  r = dpl_dbuf_consume(b, cb, 20);
  dpl_assert_int_eq(10, r); /* we only get as many as there are */
  fail_unless(0 == memcmp(cb, "abcdefghij", 10), NULL);
  dpl_assert_int_eq(0xff, cb[10]);
  dpl_assert_int_eq(0, dpl_dbuf_length(b));

  dpl_dbuf_free(b);
}
END_TEST;
#endif

Suite *
dbuf_suite()
{
  Suite *s = suite_create("dbuf");
  TCase *t = tcase_create("base");
  tcase_add_test(t, dbuf_test);
//   tcase_add_test(t, long_consume_test);
  suite_add_tcase(s, t);
  return s;
}
