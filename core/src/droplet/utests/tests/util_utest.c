/* check-sources:disable-copyright-check */
#include <unistd.h>
#include <check.h>
#include <droplet.h>
#include <droplet/utils.h>
#include <droplet/uks/uks.h>
#include "utest_main.h"

START_TEST(strrstr_test)
{
  dpl_assert_str_eq(dpl_strrstr("lev1DIRlev2DIRlev3", "DIR"), "DIRlev3");
  dpl_assert_str_eq(dpl_strrstr("foo/bar/", "/"), "/");
}
END_TEST

START_TEST(xattrs_test)
{
  char path[] = "/tmp/test_droplet_util_XXXXXX";
  dpl_dict_t* dict = NULL;
  int ret;
  char* value = NULL;

  int fd = mkstemp(path);
  dpl_assert_int_ne(-1, fd);
  ret = close(fd);
  dpl_assert_int_eq(0, ret);

  char command[1000];
  sprintf(command, "sh " SRCDIR "/tools/util_utest.sh prepare %s", path);
  ret = system(command);
  dpl_assert_int_eq(0, ret);

  /* No encoding, with prefix filter */
  dict = dpl_dict_new(13);
  dpl_assert_ptr_not_null(dict);
  ret = dpl_get_xattrs(path, dict, "user.", XATTRS_NO_ENCODING);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_count(dict);
  dpl_assert_int_eq(100, ret);
  value = dpl_dict_get_value(dict, "key042");
  dpl_assert_ptr_not_null(value);
  dpl_assert_str_eq("value042", value);
  dpl_dict_free(dict);

  /* No encoding, without prefix filter */
  dict = dpl_dict_new(13);
  dpl_assert_ptr_not_null(dict);
  ret = dpl_get_xattrs(path, dict, NULL, XATTRS_NO_ENCODING);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_count(dict);
  dpl_assert_int_eq(101, ret); /* 100 user attributes + 1 acl */
  value = dpl_dict_get_value(dict, "user.key042");
  dpl_assert_ptr_not_null(value);
  dpl_assert_str_eq("value042", value);
  dpl_dict_free(dict);

  /* Base64 encoding, without prefix filter */
  dict = dpl_dict_new(13);
  dpl_assert_ptr_not_null(dict);
  ret = dpl_get_xattrs(path, dict, NULL, XATTRS_ENCODE_BASE64);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_count(dict);
  dpl_assert_int_eq(101, ret); /* 100 user attributes + 1 acl */
  value = dpl_dict_get_value(dict, "user.key042");
  dpl_assert_ptr_not_null(value);
  dpl_assert_str_eq("dmFsdWUwNDI=", value);
  dpl_dict_free(dict);

  sprintf(command, "sh " SRCDIR "/tools/util_utest.sh clean %s", path);
  ret = system(command);
  dpl_assert_int_eq(0, ret);
}
END_TEST

START_TEST(uks_hash_setget_test)
{
  int hash_set = 0xcabefe;
  int hash_rd = 0;
  int ret;
  BIGNUM* bn = BN_new();
  dpl_assert_ptr_not_null(bn);

  BN_set_bit(bn, DPL_UKS_NBITS - 1);
  BN_clear_bit(bn, DPL_UKS_NBITS - 1);

  ret = dpl_uks_hash_set(bn, hash_set);
  dpl_assert_int_eq(ret, DPL_SUCCESS);

  hash_rd = dpl_uks_hash_get(bn);
  dpl_assert_int_eq(hash_set, hash_rd);

  BN_free(bn);
}
END_TEST

Suite* util_suite(void)
{
  Suite* s = suite_create("util");
  TCase* t = tcase_create("base");
  tcase_add_test(t, strrstr_test);
  tcase_add_test(t, xattrs_test);
  tcase_add_test(t, uks_hash_setget_test);
  suite_add_tcase(s, t);
  return s;
}
