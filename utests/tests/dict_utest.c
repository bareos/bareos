#include <check.h>
#include <droplet.h>

#include "utest_main.h"

/*
 * TODO: Conditions known to be untested:
 *
 * - passing prefix=NULL to dpl_dict_filter_{no_,}prefix() should
 *   be semantically identical to prefix="".  This will fail.
 *
 * - passing src=NULL to dpl_dict_filter_{no_,}prefix() should do
 *   nothing.  This test will fail.
 *
 * - malloc() failures anywhere (really hard to test with
 *   the check.h framework).
 *
 * - dpl_dict_get_value() looking up a missing key
 *
 * - dpl_dict_print() printing a dict inside a dict
 *
 * - passing dst=NULL to dpl_dict_copy()
 */

/* strings courtesy http://hipsteripsum.me/ */
static const char * const keys[] = {
  "Sriracha", "Banksy", "trust", "fund", "Brooklyn",
  "polaroid", "Viral", "selfies", "kogi", "Austin",
  "PBR", "stumptown", "artisan", "bespoke", "8-bit",
  "Odd", "Future", "Pinterest", "mlkshk", "McSweeney's",
  "ennui", "Wes", "Anderson" };
static const int nkeys = sizeof(keys)/sizeof(keys[0]);

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


static int
utest_dpl_dict_dump_one(dpl_dict_var_t * v, void *user_data)
{
  dpl_assert_ptr_not_null(v);
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
  dpl_assert_ptr_null(dict);
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
  dpl_assert_ptr_null(var);
  it++;
  var = dpl_dict_get(dict, *it);
  dpl_assert_ptr_not_null(var);
  value = var->val;
  fail_unless(DPL_VALUE_STRING == value->type, NULL);
  fail_unless(0 == strcmp(value->string->buf, "c"), NULL);

  it = utest_strings;
  dpl_dict_get_lowered(dict, *it, &var);
  dpl_assert_ptr_not_null(var);
  value = var->val;
  fail_unless(DPL_VALUE_STRING == value->type, NULL);
  fail_unless(0 == strcmp(value->string->buf, "a"), NULL);
  it++;
  dpl_dict_get_lowered(dict, *it, &var);
  dpl_assert_ptr_not_null(var);
  value = var->val;
  fail_unless(DPL_VALUE_STRING == value->type, NULL);
  fail_unless(0 == strcmp(value->string->buf, "b"), NULL);
  it++;
  dpl_dict_get_lowered(dict, *it, &var);
  dpl_assert_ptr_not_null(var);
  value = var->val;
  fail_unless(DPL_VALUE_STRING == value->type, NULL);
  fail_unless(0 == strcmp(value->string->buf, "c"), NULL);

  memset(pbuf, 0xff, sizeof(pbuf));
  fp = fmemopen(pbuf, sizeof(pbuf), "w");
  dpl_assert_ptr_not_null(fp);

  dpl_dict_iterate(dict, utest_dpl_dict_dump_one, fp);
  dpl_dict_print(dict, fp, 0);

  fflush(fp);
  static const char expected[] =
	"baz: c\nbar: b\nfoo: a\n"
	"baz=c\nbar=b\nfoo=a";
  fail_unless(0 == memcmp(expected, pbuf, sizeof(expected)-1), NULL);

  c = dpl_dict_count(dict);
  fail_unless(3 == c, NULL);

  dpl_dict_free(dict);
  fclose(fp);
}
END_TEST

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
  dpl_assert_ptr_not_null(dict);

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

/*
 * Test that the dpl_dict_iterate() function will return early
 * if the callback returns a non-zero number.
 */
struct break_test_state
{
  int count;
  int special_count;
  int special_code;
};

static dpl_status_t
break_test_cb(dpl_dict_var_t *var, void *arg)
{
  struct break_test_state *state = arg;
  state->count++;
  return (state->count == state->special_count ? state->special_code : DPL_SUCCESS);
}

START_TEST(iterate_break_test)
{
  dpl_dict_t *dict;
  int i;
  dpl_status_t r;
  struct break_test_state state = { 0, 0, 0 };
  char valbuf[128];

  dict = dpl_dict_new(13);
  dpl_assert_ptr_not_null(dict);

  for (i = 0 ; i < nkeys ; i++)
    {
      dpl_dict_add(dict, keys[i],
			 make_value(i, valbuf, sizeof(valbuf)),
			 /* lowered */0);
    }
  dpl_assert_int_eq(nkeys, dpl_dict_count(dict));

  /* Basic check: can iterate over everything */
  state.count = 0;	    /* initialise */
  state.special_count = 0;  /* no special return */
  state.special_code = 0;
  r = dpl_dict_iterate(dict, break_test_cb, &state);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_int_eq(nkeys, state.count);

  /* If the callback returns non-zero partway through,
   * we see that return code from dpl_dict_iterate(). */
  state.count = 0;	    /* initialise */
  state.special_count = 10; /* return an error on the 10th element */
  state.special_code = DPL_ECONNECT;
  r = dpl_dict_iterate(dict, break_test_cb, &state);
  dpl_assert_int_eq(DPL_ECONNECT, r);
  /* iteration does not proceed beyond the error return */
  dpl_assert_int_eq(10, state.count);

  /* Ditto for the 1st element */
  state.count = 0;	    /* initialise */
  state.special_count = 1; /* return an error on the 1st element */
  state.special_code = DPL_ECONNECT;
  r = dpl_dict_iterate(dict, break_test_cb, &state);
  dpl_assert_int_eq(DPL_ECONNECT, r);
  /* iteration does not proceed beyond the error return */
  dpl_assert_int_eq(1, state.count);

  /* Ditto for the last element */
  state.count = 0;	    /* initialise */
  state.special_count = nkeys; /* return an error on the last element */
  state.special_code = DPL_ECONNECT;
  r = dpl_dict_iterate(dict, break_test_cb, &state);
  dpl_assert_int_eq(DPL_ECONNECT, r);
  /* iteration does not proceed beyond the error return */
  dpl_assert_int_eq(nkeys, state.count);

  /* This also works for non-zero numbers which aren't real error codes */
  state.count = 0;	    /* initialise */
  state.special_count = 10; /* return an error on the 10th element */
  state.special_code = -511;
  r = dpl_dict_iterate(dict, break_test_cb, &state);
  dpl_assert_int_eq(-511, r);
  /* iteration does not proceed beyond the error return */
  dpl_assert_int_eq(10, state.count);

  /* This also works for positive numbers */
  state.count = 0;	    /* initialise */
  state.special_count = 10; /* return an error on the 10th element */
  state.special_code = 127;
  r = dpl_dict_iterate(dict, break_test_cb, &state);
  dpl_assert_int_eq(127, r);
  /* iteration does not proceed beyond the error return */
  dpl_assert_int_eq(10, state.count);

  dpl_dict_free(dict);
}
END_TEST

/*
 * Test replacing an existing value with dpl_dict_add(
 */
START_TEST(replace_test)
{
  dpl_dict_t *dict;
  int i;
  dpl_status_t r;
  char valbuf[128];
  /* strings courtesy http://hipsteripsum.me/ */
  static const char key0[] = "Sriracha";
  static const char val0[] = "Banksy";
  static const char val0_new[] = "polaroid";
  static const char key1[] = "trust";
  static const char val1[] = "fund";

  dict = dpl_dict_new(13);
  dpl_assert_ptr_not_null(dict);

  /* add the values */
  dpl_dict_add(dict, key0, val0, /* lowered */0);
  dpl_dict_add(dict, key1, val1, /* lowered */0);
  dpl_assert_int_eq(2, dpl_dict_count(dict));

  /* check the values are there */
  dpl_assert_str_eq(dpl_dict_get_value(dict, key0), val0);
  dpl_assert_str_eq(dpl_dict_get_value(dict, key1), val1);

  /* replace one of the values */
  dpl_dict_add(dict, key0, val0_new, /* lowered */0);

  /* check the element count is correct */
  dpl_assert_int_eq(2, dpl_dict_count(dict));

  /* check the new value is there */
  dpl_assert_str_eq(dpl_dict_get_value(dict, key0), val0_new);
  /* check the other key is unaffected */
  dpl_assert_str_eq(dpl_dict_get_value(dict, key1), val1);

  dpl_dict_free(dict);
}
END_TEST

START_TEST(remove_test)
{
  dpl_dict_t *dict;
  int i;
  dpl_status_t r;
  char valbuf[128];

  /* create with a small table to ensure we have some chains */
  dict = dpl_dict_new(5);
  dpl_assert_ptr_not_null(dict);

  /* add all the keys */
  for (i = 0 ; i < nkeys ; i++)
    {
      dpl_dict_add(dict, keys[i],
			 make_value(i, valbuf, sizeof(valbuf)),
			 /* lowered */0);
    }
  dpl_assert_int_eq(nkeys, dpl_dict_count(dict));

  /* remove the keys again */
  for (i = 0 ; i < nkeys ; i++)
    {
      dpl_dict_var_t *var = dpl_dict_get(dict, keys[i]);
      dpl_assert_ptr_not_null(var);
      dpl_assert_str_eq(var->key, keys[i]);
      dpl_dict_remove(dict, var);
      dpl_assert_int_eq(nkeys-1-i, dpl_dict_count(dict));
    }

  /* add all the keys back again */
  for (i = 0 ; i < nkeys ; i++)
    {
      dpl_dict_add(dict, keys[i],
			 make_value(i, valbuf, sizeof(valbuf)),
			 /* lowered */0);
    }
  dpl_assert_int_eq(nkeys, dpl_dict_count(dict));

  /* remove the keys again in reverse order; we do
   * this to exercise some hash chain manipulation
   * corner cases */
  for (i = nkeys-1 ; i >= 0 ; i--)
    {
      dpl_dict_var_t *var = dpl_dict_get(dict, keys[i]);
      dpl_assert_ptr_not_null(var);
      dpl_assert_str_eq(var->key, keys[i]);
      dpl_dict_remove(dict, var);
      dpl_assert_int_eq(i, dpl_dict_count(dict));
    }

  dpl_dict_free(dict);
}
END_TEST

/*
 * Test the dpl_dict_filter_prefix() and
 * dpl_dict_filter_no_prefix() functions.
 */
START_TEST(filter_test)
{
  dpl_dict_t *dict;
  dpl_dict_t *d2;
  int i;
  dpl_status_t r;
  const char *exp;
  const char *act;
  char valbuf[128];
  /* strings courtesy http://hipsteripsum.me/ */
  static const char * const keys[] = {
    "Sriracha", "mehBanksy", "trust", "fund", "Brooklyn",
    "mehpolaroid", "Viral", "mehselfies", "mehkogi", "Austin" };
  static const int nkeys = sizeof(keys)/sizeof(keys[0]);
  static const int is_meh[] = { 0, 1, 0, 0, 0,
				1, 0, 1, 1, 0 };
  static const int nmehs = 4;

  dict = dpl_dict_new(13);
  dpl_assert_ptr_not_null(dict);

  /* add all the keys */
  for (i = 0 ; i < nkeys ; i++)
    {
      dpl_dict_add(dict, keys[i],
			 make_value(i, valbuf, sizeof(valbuf)),
			 /* lowered */0);
    }
  dpl_assert_int_eq(nkeys, dpl_dict_count(dict));

  /* create a new dict and copy everything across using an empty prefix */
  d2 = dpl_dict_new(13);
  r = dpl_dict_filter_prefix(d2, dict, "");
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_int_eq(nkeys, dpl_dict_count(dict));
  dpl_assert_int_eq(nkeys, dpl_dict_count(d2));
  for (i = 0 ; i < nkeys ; i++)
    {
      exp = make_value(i, valbuf, sizeof(valbuf));
      act = dpl_dict_get_value(d2, keys[i]);
      dpl_assert_str_eq(exp, act);
    }
  dpl_dict_free(d2);

  /* create a new dict and copy just the matching ones */
  d2 = dpl_dict_new(13);
  r = dpl_dict_filter_prefix(d2, dict, "meh");
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_int_eq(nkeys, dpl_dict_count(dict));
  dpl_assert_int_eq(nmehs, dpl_dict_count(d2));
  for (i = 0 ; i < nkeys ; i++)
    {
      if (is_meh[i])
	{
	  exp = make_value(i, valbuf, sizeof(valbuf));
	  /*
	   * dpl_dict_filter_prefix() inexplicably removes the prefix
	   * from the keys it copies across.  This was probably very
	   * convenient for the person who wrote the code, but isn't
	   * all that easy to explain.  But this is why we add a
	   * strlen() here.
	   */
	  act = dpl_dict_get_value(d2, keys[i]+strlen("meh"));
	  dpl_assert_str_eq(exp, act);
	}
    }
  dpl_dict_free(d2);

  /* create a new dict and copy just the non-matching ones */
  d2 = dpl_dict_new(13);
  r = dpl_dict_filter_no_prefix(d2, dict, "meh");
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_int_eq(nkeys, dpl_dict_count(dict));
  dpl_assert_int_eq(nkeys-nmehs, dpl_dict_count(d2));
  for (i = 0 ; i < nkeys ; i++)
    {
      if (!is_meh[i])
	{
	  exp = make_value(i, valbuf, sizeof(valbuf));
	  act = dpl_dict_get_value(d2, keys[i]);
	  dpl_assert_str_eq(exp, act);
	}
    }
  dpl_dict_free(d2);

  dpl_dict_free(dict);
}
END_TEST

/*
 * Test the "lowered" feature, which is almost but not quite
 * the same as case insensitivity.
 */
START_TEST(lowered_test)
{
  dpl_dict_t *dict;
  dpl_status_t r;
  const char *s;
  dpl_dict_var_t *var;
  const char value[] = "World";
  const char value2[] = "Mondo";
  const char value3[] = "Monde";

  dict = dpl_dict_new(13);
  dpl_assert_ptr_not_null(dict);

  /* Add a value, lowered */
  r = dpl_dict_add(dict, "Hello", value, /*lowered*/1);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_int_eq(1, dpl_dict_count(dict));

  /* The actual key used is internally lowercased, so
   * doing a normal get on the key originally given
   * should fail */
  dpl_assert_str_eq(NULL, dpl_dict_get_value(dict, "Hello"));

  /* Likewise, doing a normal get on a lowercase version
   * of the original key should succeed */
  dpl_assert_str_eq(value, dpl_dict_get_value(dict, "hello"));

  /* dpl_dict_get_lowered internally lowercases the key it's
   * given, so it can be used with keys of any casing */

  var = BADPOINTER;
  r = dpl_dict_get_lowered(dict, "hello", &var);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_ptr_not_null(var);
  dpl_assert_ptr_ne(var, BADPOINTER);
  dpl_assert_str_eq(value, dpl_sbuf_get_str(var->val->string));

  var = BADPOINTER;
  r = dpl_dict_get_lowered(dict, "Hello", &var);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_ptr_not_null(var);
  dpl_assert_ptr_ne(var, BADPOINTER);
  dpl_assert_str_eq(value, dpl_sbuf_get_str(var->val->string));

  var = BADPOINTER;
  r = dpl_dict_get_lowered(dict, "HELLO", &var);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_ptr_not_null(var);
  dpl_assert_ptr_ne(var, BADPOINTER);
  dpl_assert_str_eq(value, dpl_sbuf_get_str(var->val->string));

  var = BADPOINTER;
  r = dpl_dict_get_lowered(dict, "hElLo", &var);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_ptr_not_null(var);
  dpl_assert_ptr_ne(var, BADPOINTER);
  dpl_assert_str_eq(value, dpl_sbuf_get_str(var->val->string));

  /* check that dpl_dict_get_lowered() will report as missing
   * some keys that we know not to be present */

  var = BADPOINTER;
  r = dpl_dict_get_lowered(dict, "HellonEarth", &var);
  dpl_assert_int_eq(DPL_ENOENT, r);
  /* the value of `var' on failure is not documented */

  var = BADPOINTER;
  r = dpl_dict_get_lowered(dict, "Hell", &var);
  dpl_assert_int_eq(DPL_ENOENT, r);
  /* the value of `var' on failure is not documented */

  var = BADPOINTER;
  r = dpl_dict_get_lowered(dict, "daffyduck", &var);
  dpl_assert_int_eq(DPL_ENOENT, r);
  /* the value of `var' on failure is not documented */

  /* Verify that inserting another key which maps to the
   * same lowercased string, replaces the first key */

  r = dpl_dict_add(dict, "hello", value2, /*lowered*/1);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_int_eq(1, dpl_dict_count(dict));
  dpl_assert_str_eq(value2, dpl_dict_get_value(dict, "hello"));

  r = dpl_dict_add(dict, "hELLo", value3, /*lowered*/1);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_int_eq(1, dpl_dict_count(dict));
  dpl_assert_str_eq(value3, dpl_dict_get_value(dict, "hello"));

  dpl_dict_free(dict);
}
END_TEST

Suite *
dict_suite(void)
{
  Suite *s = suite_create("dict");
  TCase *d = tcase_create("base");
  tcase_add_test(d, dict_test);
  tcase_add_test(d, long_key_test);
  tcase_add_test(d, iterate_break_test);
  tcase_add_test(d, replace_test);
  tcase_add_test(d, remove_test);
  tcase_add_test(d, filter_test);
  tcase_add_test(d, lowered_test);
  suite_add_tcase(s, d);
  return s;
}


