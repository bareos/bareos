#include <stdio.h>
#include <check.h>
#include <droplet.h>
#include "utest_main.h"


START_TEST(sbuf_test)
{
  dpl_sbuf_t	*b;
  dpl_sbuf_t    *b2;
  FILE		*fp;
  char		pbuf[1024];

  int sizes[] = { 1, 2, 4, 8, 16, 32, 65};
  unsigned int  i;
  for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
      b = dpl_sbuf_new(sizes[i]);
      dpl_assert_ptr_not_null(b);
      b2 = dpl_sbuf_dup(b);
      dpl_assert_ptr_not_null(b2);
      dpl_sbuf_free(b);
      dpl_sbuf_free(b2);
    }

  memset(pbuf, 0xff, sizeof(pbuf));
  fp = fmemopen(pbuf, sizeof(pbuf), "w");
  dpl_assert_ptr_not_null(fp);

  const char  * strs[] = { "truc", "bidule", "machin", "chose" };
  for (i = 0; i < sizeof(strs) / sizeof(strs[0]); i++)
    {
      b = dpl_sbuf_new_from_str(strs[i]);
      dpl_assert_ptr_not_null(b);
      dpl_assert_str_eq(strs[i], dpl_sbuf_get_str(b));
      b2 = dpl_sbuf_dup(b);
      dpl_assert_ptr_not_null(b2);
      dpl_assert_str_eq(strs[i], dpl_sbuf_get_str(b2));
      dpl_sbuf_print(fp, b);
      dpl_sbuf_free(b);
      dpl_sbuf_free(b2);
    }

  const char    * full_str = "trucbidulemachinchose";
  fflush(fp);
  fail_unless(0 == memcmp(full_str, pbuf, strlen(full_str)), NULL);

  b = dpl_sbuf_new_from_str("");
  for (i = 0; i < sizeof(strs) / sizeof(strs[0]); i++)
    {
      dpl_status_t  ret;
      ret = dpl_sbuf_add_str(b, strs[i]);
      fail_unless(DPL_SUCCESS == ret, NULL);
    }
  dpl_assert_str_eq(full_str, dpl_sbuf_get_str(b));
  dpl_sbuf_free(b);
  fclose(fp);
}
END_TEST


Suite *
sbuf_suite()
{
  Suite *s = suite_create("sbuf");
  TCase *t = tcase_create("base");
  tcase_add_test(t, sbuf_test);
  suite_add_tcase(s, t);
  return s;
}
