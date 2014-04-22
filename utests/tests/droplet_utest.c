/* unit test the code in droplet.c */
#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <check.h>
#include <droplet.h>

#include "utest_main.h"

START_TEST(status_str_test)
{
  dpl_assert_str_eq("DPL_SUCCESS", dpl_status_str(0));
  dpl_assert_str_eq("DPL_SUCCESS", dpl_status_str(DPL_SUCCESS));
  dpl_assert_str_eq("DPL_FAILURE", dpl_status_str(DPL_FAILURE));
  /* lots of others, just try one */
  dpl_assert_str_eq("DPL_ECONNECT", dpl_status_str(DPL_ECONNECT));
  /* we get a non-null string when passing completely bogus error codes */
  dpl_assert_ptr_not_null(dpl_status_str(3000));
  dpl_assert_ptr_not_null(dpl_status_str(-3000));
}
END_TEST

START_TEST(init_test)
{
  dpl_init();
  dpl_free();
}
END_TEST

START_TEST(option_test)
{
  dpl_status_t r;
  dpl_option_t opt;
  dpl_option_t *o2;

  /* the API has no initialiser macro or dpl_option_new() */
  memset(&opt, 0xff, sizeof(opt));

  /* testcases where parsing fails */
#define TESTCASE(str, err) \
  { \
    r = dpl_parse_option(str, &opt); \
    dpl_assert_int_eq(err, r); \
  }

  /* colons are necessary (which is dumb, but there it is) */
  TESTCASE("lazy", DPL_EINVAL);
  /* a bogus option name fails cleanly */
  TESTCASE("i_am_so_bogus:", DPL_EINVAL);

#undef TESTCASE
  /* testcases where parsing succeeds */
#define TESTCASE(str, maskval, expvers, forvers) \
  r = dpl_parse_option(str, &opt); \
  dpl_assert_int_eq(DPL_SUCCESS, r); \
  dpl_assert_int_eq(maskval, opt.mask); \
  dpl_assert_str_eq(expvers, opt.expect_version); \
  dpl_assert_str_eq(forvers, opt.force_version); \
  o2 = dpl_option_dup(&opt); \
  dpl_assert_int_eq(maskval, o2->mask); \
  dpl_assert_str_eq(expvers, o2->expect_version); \
  dpl_assert_str_eq(forvers, o2->force_version); \
  dpl_option_free(o2)

  /* an empty string is a valid set of options */
  TESTCASE("", 0, "", "");
  /* run through all the options to make sure they are parsed correctly */
  TESTCASE("lazy:", DPL_OPTION_LAZY, "", "");
  TESTCASE("http_compat:", DPL_OPTION_HTTP_COMPAT, "", "");
  TESTCASE("raw:", DPL_OPTION_RAW, "", "");
  TESTCASE("append_metadata:", DPL_OPTION_APPEND_METADATA, "", "");
  TESTCASE("consistent:", DPL_OPTION_CONSISTENT, "", "");
  TESTCASE("expect_version:123", DPL_OPTION_EXPECT_VERSION, "123", "");
  TESTCASE("force_version:123", DPL_OPTION_FORCE_VERSION, "", "123");
  /* two options separated by various separators */
  TESTCASE("lazy: http_compat:", DPL_OPTION_LAZY|DPL_OPTION_HTTP_COMPAT, "", "");
  TESTCASE("lazy:,http_compat:", DPL_OPTION_LAZY|DPL_OPTION_HTTP_COMPAT, "", "");
  TESTCASE("lazy:;http_compat:", DPL_OPTION_LAZY|DPL_OPTION_HTTP_COMPAT, "", "");
  /* many options */
  TESTCASE("lazy: http_compat: raw: append_metadata: consistent:",
	   DPL_OPTION_LAZY|DPL_OPTION_HTTP_COMPAT|DPL_OPTION_RAW|DPL_OPTION_APPEND_METADATA|DPL_OPTION_CONSISTENT,
	   "", "");
  /* order doesn't matter */
  TESTCASE("http_compat: lazy:", DPL_OPTION_LAZY|DPL_OPTION_HTTP_COMPAT, "", "");
  TESTCASE("append_metadata: http_compat: raw: consistent: lazy:",
	   DPL_OPTION_LAZY|DPL_OPTION_HTTP_COMPAT|DPL_OPTION_RAW|DPL_OPTION_APPEND_METADATA|DPL_OPTION_CONSISTENT,
	   "", "");

#undef TESTCASE
}
END_TEST

START_TEST(condition_test)
{
  dpl_status_t r;
  int i;
  dpl_condition_t cond;
  dpl_condition_t *c2;

  /* the API has no initialiser macro or dpl_condition_new() */
  memset(&cond, 0xff, sizeof(cond));

  /* testcases where parsing fails */
#define TESTCASE(str, err) \
  { \
    r = dpl_parse_condition(str, &cond); \
    dpl_assert_int_eq(err, r); \
  }

  /* colons are necessary (which is dumb, but there it is) */
  TESTCASE("if-match", DPL_EINVAL);
  /* a bogus option name fails cleanly */
  TESTCASE("i_am_so_bogus:", DPL_EINVAL);
  /* an etag which is too long */
  TESTCASE("if-none-match:ecf3245105b18821eda700c279ba9ae39af72c169574ea47e3c37037744fb444710f4f31d655894305f380b0a8c1aa83", DPL_EINVAL);
  /* an invalid time string */
  TESTCASE("if-modified-since:356-Flubuary-20789", DPL_EINVAL);
  /* too many conditions */
  TESTCASE("if-match:a if-match:b if-match:c if-match:d if-match:e "
	   "if-match:f if-match:g if-match:h if-match:i if-match:j "
	   "if-match:k",
	   DPL_ENAMETOOLONG);

#undef TESTCASE
  /* testcases where parsing succeeds */
#define TESTCASE(str, ...) \
  { \
    const dpl_condition_one_t _e[] = { __VA_ARGS__ }; \
    int _n = sizeof(_e)/sizeof(_e[0]); \
    r = dpl_parse_condition(str, &cond); \
    dpl_assert_int_eq(DPL_SUCCESS, r); \
    dpl_assert_int_eq(_n, cond.n_conds); \
    for (i=0 ; i<_n ; i++) \
      { \
	dpl_assert_int_eq(_e[i].type, cond.conds[i].type); \
	dpl_assert_int_eq(_e[i].time, cond.conds[i].time); \
	dpl_assert_str_eq(_e[i].etag, cond.conds[i].etag); \
      } \
    c2 = dpl_condition_dup(&cond); \
    dpl_assert_ptr_not_null(c2); \
    dpl_assert_int_eq(0, memcmp(c2, &cond, sizeof(cond))); \
    dpl_condition_free(c2); \
  }

  /* an empty string is a valid set of conditions */
  TESTCASE("");
  /* run through all the conditions to make sure they are parsed correctly */
  TESTCASE("if-match:5eaebe6fcda83fa5c9904836314df05d",
	   { .type = DPL_CONDITION_IF_MATCH,
	   .etag = "5eaebe6fcda83fa5c9904836314df05d"});
  TESTCASE("if-none-match:3381508f73d74f53a76a8d748f4b2a6d",
	   { .type = DPL_CONDITION_IF_NONE_MATCH,
	   .etag = "3381508f73d74f53a76a8d748f4b2a6d"});
  TESTCASE("if-modified-since:15-Oct-2010()03:19:52()+1100",
	   { .type = DPL_CONDITION_IF_MODIFIED_SINCE,
	   .time = 1287073192L});
  TESTCASE("if-unmodified-since:5-Oct-2010()03:19:52()+1100",
	   { .type = DPL_CONDITION_IF_UNMODIFIED_SINCE,
	   .time = 1286209192L});
  /* multiple conditions */
  TESTCASE("if-match:a if-match:b",
	   { .type = DPL_CONDITION_IF_MATCH, .etag = "a"},
	   { .type = DPL_CONDITION_IF_MATCH, .etag = "b"});
  TESTCASE("if-match:a if-none-match:b if-match:c",
	   { .type = DPL_CONDITION_IF_MATCH, .etag = "a"},
	   { .type = DPL_CONDITION_IF_NONE_MATCH, .etag = "b"},
	   { .type = DPL_CONDITION_IF_MATCH, .etag = "c"});

#undef TESTCASE
}
END_TEST

Suite *
droplet_suite()
{
  Suite *s = suite_create("droplet");
  TCase *t = tcase_create("base");
  tcase_add_test(t, status_str_test);
  tcase_add_test(t, init_test);
  tcase_add_test(t, option_test);
  tcase_add_test(t, condition_test);
  suite_add_tcase(s, t);
  return s;
}
