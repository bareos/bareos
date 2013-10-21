#include <check.h>
#include <droplet.h>

#include "utest_main.h"


static int
utest_dpl_dict_dump_one(dpl_dict_var_t * v, void *user_data)
{
  fail_if(NULL == v, NULL);
  dpl_assert_int_eq(v->val->type, DPL_VALUE_STRING);
  fprintf((FILE *)user_data, "%s: %s\n", v->key, v->val->string->buf);

  return 0;
}

START_TEST(dict_test)
{
  dpl_dict_t             *dict;
  const char * const     *it;
  char                    init_value[] = "a";
  dpl_dict_var_t         *var;
  const dpl_value_t      *value;
  dpl_status_t            ret;
  int                     c;
  FILE			 *fp = NULL;
  char			  pbuf[1024];

  static const char * const utest_strings[] = { "Foo", "bAr", "baz", NULL };

  dict = dpl_dict_new(0);
  fail_unless(NULL == dict, NULL);
  dict = dpl_dict_new(10);

  it = utest_strings;
  while (*it != NULL)
    {
      ret = dpl_dict_add(dict, *it, init_value, 1);
      fail_unless(DPL_SUCCESS == ret, NULL);
      (*init_value)++;
      it++;
    }
    //
    // new, free, dup, copy combo
    //
  dpl_dict_t * d2 = dpl_dict_new(5);
  ret = dpl_dict_copy(d2, dict);
  fail_unless(DPL_SUCCESS == ret, NULL);
  dpl_dict_free(dict);
  dict = dpl_dict_new(20);
  ret = dpl_dict_copy(dict, d2);
  fail_unless(DPL_SUCCESS == ret, NULL);
  dpl_dict_free(d2);
  d2 = dpl_dict_dup(dict);
  fail_unless(NULL != d2, NULL);
  dpl_dict_free(dict);
  dict = d2;

  it = utest_strings;
  var = dpl_dict_get(dict, *it);
  fail_unless(var == NULL, NULL);
  it++;
  var = dpl_dict_get(dict, *it);
  fail_unless(var == NULL, NULL);
  it++;
  var = dpl_dict_get(dict, *it);
  fail_if(NULL == var, NULL);
  value = var->val;
  fail_unless(DPL_VALUE_STRING == value->type, NULL);
  fail_unless(0 == strcmp(value->string->buf, "c"), NULL);

  it = utest_strings;
  dpl_dict_get_lowered(dict, *it, &var);
  fail_if(NULL == var, NULL);
  value = var->val;
  fail_unless(DPL_VALUE_STRING == value->type, NULL);
  fail_unless(0 == strcmp(value->string->buf, "a"), NULL);
  it++;
  dpl_dict_get_lowered(dict, *it, &var);
  fail_if(NULL == var, NULL);
  value = var->val;
  fail_unless(DPL_VALUE_STRING == value->type, NULL);
  fail_unless(0 == strcmp(value->string->buf, "b"), NULL);
  it++;
  dpl_dict_get_lowered(dict, *it, &var);
  fail_if(NULL == var, NULL);
  value = var->val;
  fail_unless(DPL_VALUE_STRING == value->type, NULL);
  fail_unless(0 == strcmp(value->string->buf, "c"), NULL);

  memset(pbuf, 0xff, sizeof(pbuf));
  fp = fmemopen(pbuf, sizeof(pbuf), "w");
  fail_if(NULL == fp, NULL);

  dpl_dict_iterate(dict, utest_dpl_dict_dump_one, fp);
  dpl_dict_print(dict, fp, 0);

  fflush(fp);
  static const char expected[] =
	"baz: c\nbar: b\nfoo: a\n"
	"baz=c\nbar=b\nfoo=a";
  fail_unless(0 == memcmp(expected, pbuf, sizeof(expected)-1), NULL);

  c = dpl_dict_count(dict);
  fail_unless(3 == c, NULL);

#if 0
  dpl_dict_var_free();
  dpl_dict_add_value();
  dpl_dict_remove();
  dpl_dict_dup();
  dpl_dict_filter_prefix();
  dpl_dict_get_value();
#endif

  dpl_dict_free(dict);
  fclose(fp);
}
END_TEST

static const char *
make_key(int i, char *buf, size_t maxlen)
{
  int j;
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz";
  if (i >= maxlen)
    return NULL;
  for (j = 0 ; j < i ; j++)
    buf[j] = chars[j % (sizeof(chars)-1)];
  buf[j] = '\0';
  return buf;
}

static const char *
make_value(int i, char *buf, size_t maxlen)
{
    snprintf(buf, maxlen, "X%dY", i);
    return buf;
}

/*
 * Use the dict with long key strings; tests
 * some corner cases e.g. in the hash function.
 */
START_TEST(long_key_test)
{
#define N 512
  dpl_dict_t *dict;
  int i;
  int j;
  const char *act;
  const char *exp;
  char keybuf[1024];
  char valbuf[1024];

  dict = dpl_dict_new(13);
  fail_if(NULL == dict, NULL);

  for (i = 0 ; i < N ; i++)
    {
      dpl_dict_add(dict, make_key(i, keybuf, sizeof(keybuf)),
			 make_value(i, valbuf, sizeof(valbuf)),
			 /* lowered */0);
    }
  dpl_assert_int_eq(N, dpl_dict_count(dict));

  for (i = 0 ; i < N ; i++)
    {
      act = dpl_dict_get_value(dict, make_key(i, keybuf, sizeof(keybuf)));
      exp = make_value(i, valbuf, sizeof(valbuf));
      dpl_assert_str_eq(act, exp);
    }

  dpl_dict_free(dict);
#undef N
}
END_TEST


Suite *
dict_suite(void)
{
  Suite *s = suite_create("dict");
  TCase *d = tcase_create("base");
  tcase_add_test(d, dict_test);
  tcase_add_test(d, long_key_test);
  suite_add_tcase(s, d);
  return s;
}


