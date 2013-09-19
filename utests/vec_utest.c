#include <check.h>
#include <droplet.h>

static int reverse_sort (const void * a, const void * b)
{
  unsigned int  x = (unsigned int) a;
  unsigned int  y = (unsigned int) b;

  return (x == y) ? 0 : (x < y ? 1 : -1);
}


START_TEST(vec_test)
{
  unsigned int    i, j;
  dpl_vec_t     * v, * v2;
  dpl_status_t    ret;

  v = dpl_vec_new(0, 0);
  fail_unless(NULL == v, NULL);

  v = dpl_vec_new(100, 10);
  fail_if(NULL == v, NULL);
  dpl_vec_free(v);

  int   ics[] = {0, 10};
  for (i = 0; i < sizeof(ics) / sizeof(ics[0]); i++)
    {
      v = dpl_vec_new(1, ics[i]);
      for (j = 0; j < 1000; j++)
        {
          ret = dpl_vec_add(v, (void*)j);
          fail_unless(DPL_SUCCESS == ret, NULL);
        }
      v2 = dpl_vec_dup(v);
      for (j = 0; j < 1000; j++)
        {
          unsigned int  t;
          t = (unsigned int) dpl_vec_get(v2, j);
          fail_unless((unsigned int) t == j, NULL);
        }
      dpl_vec_print(v, stdout, 0);
      puts("");
      dpl_vec_free(v);
      dpl_vec_free(v2);
    }

  v = dpl_vec_new(1000, 0);
  for (i = 0; i < 1000; i++)
    {
      ret = dpl_vec_add(v, (void*) i);
      fail_unless(DPL_SUCCESS == ret, NULL);
    }
  dpl_vec_sort(v, reverse_sort);
  for (i = 0; i < 1000; i++)
    {
      j = (unsigned int) dpl_vec_get(v, i);
      fail_unless(j == (1000 - 1 - i), NULL);
    }
}
END_TEST


    Suite *
vec_suite (void)
{
  Suite * s = suite_create("vec");
  TCase * t = tcase_create("base");
  tcase_add_test(t, vec_test);
  suite_add_tcase(s, t);
  return s;
}

