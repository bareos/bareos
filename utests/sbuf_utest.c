#include <stdio.h>
#include <check.h>
#include <droplet.h>


START_TEST(sbuf_test)
{
  dpl_sbuf_t    * b;
  dpl_sbuf_t    * b2;

  int sizes[] = { 1, 2, 4, 8, 16, 32, 65};
  unsigned int  i;
  for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
      b = dpl_sbuf_new(sizes[i]);
      fail_if(NULL == b, NULL);
      b2 = dpl_sbuf_dup(b);
      fail_if(NULL == b2, NULL);
      dpl_sbuf_free(b);
      dpl_sbuf_free(b2);
    }

  const char  * strs[] = { "truc", "bidule", "machin", "chose" };
  for (i = 0; i < sizeof(strs) / sizeof(strs[0]); i++)
    {
      b = dpl_sbuf_new_from_str(strs[i]);
      fail_if(NULL == b, NULL);
      fail_unless(0 == strcmp(strs[i], dpl_sbuf_get_str(b)), NULL);
      b2 = dpl_sbuf_dup(b);
      fail_if(NULL == b2, NULL);
      fail_unless(0 == strcmp(strs[i], dpl_sbuf_get_str(b2)), NULL);
      dpl_sbuf_print(stdout, b);
      dpl_sbuf_free(b);
      dpl_sbuf_free(b2);
    }

  const char    * full_str = "trucbidulemachinchose";
  b = dpl_sbuf_new_from_str("");
  for (i = 0; i < sizeof(strs) / sizeof(strs[0]); i++)
    {
      dpl_status_t  ret;
      ret = dpl_sbuf_add_str(b, strs[i]);
      fail_unless(DPL_SUCCESS == ret, NULL);
    }
  fail_unless(0 == strcmp(full_str, dpl_sbuf_get_str(b)), NULL);
  dpl_sbuf_free(b);
  puts("");
}
END_TEST


    Suite *
sbuf_suite ()
{
  Suite * s = suite_create("sbuf");
  TCase * t = tcase_create("base");
  tcase_add_test(t, sbuf_test);
  suite_add_tcase(s, t);
  return s;
}
