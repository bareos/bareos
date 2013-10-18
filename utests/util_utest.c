#include <check.h>
#include <droplet.h>
#include <droplet/utils.h>
#include "utest_main.h"

START_TEST(strrstr_test)
{
  dpl_assert_str_eq(dpl_strrstr("lev1DIRlev2DIRlev3", "DIR"), "DIRlev3");
  dpl_assert_str_eq(dpl_strrstr("foo/bar/", "/"), "/");
}
END_TEST

Suite *
util_suite(void)
{
  Suite *s = suite_create("util");
  TCase *t = tcase_create("base");
  tcase_add_test(t, strrstr_test);
  suite_add_tcase(s, t);
  return s;
}

