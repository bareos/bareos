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

static int check_keys (const char   * k,
                       int            kl,
                       void         * arg)
{
  arg_list      * a = (arg_list*) arg;
  unsigned int    i;
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


START_TEST(ntinydb_test)
{
  dpl_sbuf_t    * b = dpl_sbuf_new(1);
  const char    * keys[] = { "a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
      "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"};  
  char          * datas[sizeof(keys) / sizeof(keys[0])];
  unsigned int    i;
  int             fd;
  int             ret;
  dpl_status_t    s;

  fd = open("/dev/urandom", O_RDONLY);
  fail_if(-1 == ret, NULL);

  for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
      datas[i] = malloc(512);
      fail_if(NULL == datas[i], NULL);
      ret = read(fd, datas[i], 512);
      fail_unless(512 == ret, NULL);
      s = dpl_ntinydb_set(b, keys[i], datas[i], 512);
      fail_unless(DPL_SUCCESS == s, NULL);
    }
  for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
      const char    * datas_check[sizeof(keys) / sizeof(keys[0])];
      int             out_len;
      s = dpl_ntinydb_get(b->buf, b->len, keys[i], datas_check + i, &out_len);
      fail_unless(DPL_SUCCESS == s, NULL);
      fail_unless(512 == out_len);
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
  fail_unless(DPL_SUCCESS == s, NULL);
  for (i = 0; i < all_keys.nelem; i++)
    {
      fail_if(false == all_keys.found[i], NULL);
    }
}
END_TEST


    Suite *
ntinydb_suite (void)
{
  Suite * s = suite_create("ntinydb");
  TCase * d = tcase_create("base");
  tcase_add_test(d, ntinydb_test);
  suite_add_tcase(s, d);
  return s;
}

