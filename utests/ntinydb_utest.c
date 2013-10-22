#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <check.h>
#include <droplet.h>
#include <droplet/ntinydb.h>

#include "utest_main.h"

typedef struct {
    unsigned int       nelem;
    const char      ** keys;
    bool             * found;
} arg_list;

static int check_keys(const char *k,
		      int kl,
		      void *arg)
{
  arg_list	    *a = (arg_list*) arg;
  unsigned int	    i;
  for (i = 0; i < a->nelem; i++)
    {
      if (0 == strncmp(k, a->keys[i], kl))
        {
          a->found[i] = true;
          break;
        }
    }
  return 0;
}

static char *
new_pseudorandom_bytes(int n)
{
  unsigned char *x;
  int i;
  unsigned long bits;
  int shift = -1;

  x = malloc(n);
  dpl_assert_ptr_not_null(x);
  for (i = 0 ; i < n ; i++)
    {
      if (shift < 0)
	{
	  bits = lrand48();
	  /* lrand48() always returns 32b regardless of sizeof(long) */
	  shift = 24;
	}
      x[i] = (bits >> shift) & 0xff;
      shift -= 8;
    }
  return (char *)x;
}


START_TEST(ntinydb_test)
{
  dpl_sbuf_t    * b = dpl_sbuf_new(1);
  const char    * keys[] = { "a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
      "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"};  
  char          * datas[sizeof(keys) / sizeof(keys[0])];
  unsigned int    i;
  int             ret;
  dpl_status_t    s;

  srand48(0xdeadbeef);

  for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
      datas[i] = new_pseudorandom_bytes(512);
      s = dpl_ntinydb_set(b, keys[i], datas[i], 512);
      dpl_assert_int_eq(DPL_SUCCESS, s);
    }
  for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
      const char    * datas_check[sizeof(keys) / sizeof(keys[0])];
      int             out_len;
      s = dpl_ntinydb_get(b->buf, b->len, keys[i], datas_check + i, &out_len);
      dpl_assert_int_eq(DPL_SUCCESS, s);
      dpl_assert_int_eq(512, out_len);
      fail_unless(0 == memcmp(datas_check[i], datas[i], 512));
    }
  arg_list  all_keys;
  all_keys.nelem = sizeof(keys) / sizeof(keys[0]);
  all_keys.keys = keys;
  all_keys.found = malloc(sizeof(bool) * all_keys.nelem);
  for (i = 0; i < all_keys.nelem; i++)
    {
      all_keys.found[i] = false;
    }

  s = dpl_ntinydb_list(b->buf, b->len, check_keys, &all_keys);
  dpl_assert_int_eq(DPL_SUCCESS, s);
  for (i = 0; i < all_keys.nelem; i++)
    {
      dpl_assert_int_eq(true, all_keys.found[i]);
    }
  free(all_keys.found);

  for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
      free(datas[i]);
  dpl_sbuf_free(b);
}
END_TEST


Suite *
ntinydb_suite(void)
{
  Suite *s = suite_create("ntinydb");
  TCase *d = tcase_create("base");
  tcase_add_test(d, ntinydb_test);
  suite_add_tcase(s, d);
  return s;
}

