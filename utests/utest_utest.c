#include <check.h>
#include <stdlib.h>
#include "utest_main.h"

static int three = 3;
static int two_plus_one = 3;
static int forty_two = 42;
static const char *ford = "Ford";
static const char *henrys_company = "Ford";
static const char *gmc = "General Motors";

/* Test the dpl_assert_*() macros in utest_main.h */

/*
 * This test has all the checks that are supposed to pass
 */
START_TEST(pass_test)
{
    dpl_assert_int_eq(three, two_plus_one);
    dpl_assert_int_eq(two_plus_one, three);
    dpl_assert_int_eq(three, 3);
    dpl_assert_int_eq(3, three);

    dpl_assert_int_ne(forty_two, three);
    dpl_assert_int_ne(three, forty_two);
    dpl_assert_int_ne(forty_two, 5);
    dpl_assert_int_ne(5, forty_two);

    dpl_assert_str_eq(henrys_company, ford);
    dpl_assert_str_eq(ford, henrys_company);
    dpl_assert_str_eq(ford, "Ford");
    dpl_assert_str_eq("Ford", ford);

    dpl_assert_str_ne(gmc, ford);
    dpl_assert_str_ne(ford, gmc);
    dpl_assert_str_ne(ford, "Chrysler");
    dpl_assert_str_ne("Chrysler", ford);
}
END_TEST

/*
 * These tests have all the checks that are supposed to fail.
 * Limitations in the check library make it hard to automate
 * running these in "make check" so we allow manual testing
 * by running
 *
 * DPL_UTEST_UTEST_DO_FAILS=yes make check
 *
 * The expected result is that all of fail1..fail16 will fail.
 */

START_TEST(fail1_test)
{
    dpl_assert_int_ne(three, two_plus_one);
}
END_TEST

START_TEST(fail2_test)
{
    dpl_assert_int_ne(two_plus_one, three);
}
END_TEST

START_TEST(fail3_test)
{
    dpl_assert_int_ne(three, 3);
}
END_TEST

START_TEST(fail4_test)
{
    dpl_assert_int_ne(3, three);
}
END_TEST

START_TEST(fail5_test)
{
    dpl_assert_int_eq(forty_two, three);
}
END_TEST

START_TEST(fail6_test)
{
    dpl_assert_int_eq(three, forty_two);
}
END_TEST

START_TEST(fail7_test)
{
    dpl_assert_int_eq(forty_two, 5);
}
END_TEST

START_TEST(fail8_test)
{
    dpl_assert_int_eq(5, forty_two);
}
END_TEST


START_TEST(fail9_test)
{
    dpl_assert_str_ne(henrys_company, ford);
}
END_TEST

START_TEST(fail10_test)
{
    dpl_assert_str_ne(ford, henrys_company);
}
END_TEST

START_TEST(fail11_test)
{
    dpl_assert_str_ne(ford, "Ford");
}
END_TEST

START_TEST(fail12_test)
{
    dpl_assert_str_ne("Ford", ford);
}
END_TEST

START_TEST(fail13_test)
{
    dpl_assert_str_eq(gmc, ford);
}
END_TEST

START_TEST(fail14_test)
{
    dpl_assert_str_eq(ford, gmc);
}
END_TEST

START_TEST(fail15_test)
{
    dpl_assert_str_eq(ford, "Chrysler");
}
END_TEST

START_TEST(fail16_test)
{
    dpl_assert_str_eq("Chrysler", ford);
}
END_TEST


Suite *
utest_suite(void)
{
  Suite *s = suite_create("utest");
  TCase *t = tcase_create("pass");
  tcase_add_test(t, pass_test);
  if (getenv("DPL_UTEST_UTEST_DO_FAILS"))
    {
      tcase_add_test(t, fail1_test);
      tcase_add_test(t, fail2_test);
      tcase_add_test(t, fail3_test);
      tcase_add_test(t, fail4_test);
      tcase_add_test(t, fail5_test);
      tcase_add_test(t, fail6_test);
      tcase_add_test(t, fail7_test);
      tcase_add_test(t, fail8_test);
      tcase_add_test(t, fail9_test);
      tcase_add_test(t, fail10_test);
      tcase_add_test(t, fail11_test);
      tcase_add_test(t, fail12_test);
      tcase_add_test(t, fail13_test);
      tcase_add_test(t, fail14_test);
      tcase_add_test(t, fail15_test);
      tcase_add_test(t, fail16_test);
    }
  suite_add_tcase(s, t);
  return s;
}

