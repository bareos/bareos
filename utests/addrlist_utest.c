#include <droplet.h>
#include <check.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "utest_main.h"

/* create an empty addrlist */
START_TEST(empty_test)
{
  dpl_addrlist_t *addrlist;
  char *hoststr = NULL;
  char *portstr = NULL;
  struct in_addr ipaddr;
  u_short port = 0;
  char *s = NULL;

  memset(&ipaddr, 0, sizeof(ipaddr));

  addrlist = dpl_addrlist_create("4244");
  dpl_assert_ptr_not_null(addrlist);

  /* an empty addrlist has a zero count */
  dpl_assert_int_eq(0, dpl_addrlist_count(addrlist));

  /* an empty addrlist has no elements to get */
  dpl_assert_int_eq(DPL_ENOENT, dpl_addrlist_get_nth(addrlist, 0, &hoststr,
						     &portstr, &ipaddr, &port));

  /* an empty addrlist is described as a non-NULL empty string */
  s = dpl_addrlist_get(addrlist);
  dpl_assert_str_eq(s, "");

  free(s);
  dpl_addrlist_free(addrlist);
}
END_TEST

START_TEST(default_default_port_test)
{
  dpl_addrlist_t *addrlist;
  dpl_status_t r;
  char *s;

  /* create the addrlist */
  addrlist = dpl_addrlist_create(NULL);
  dpl_assert_ptr_not_null(addrlist);

  r = dpl_addrlist_add_from_str(addrlist, "192.168.1.1");
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_int_eq(1, dpl_addrlist_count(addrlist));

  /* verify the string form of the addrlist */
  s = dpl_addrlist_get(addrlist);
  dpl_assert_str_eq(s, "192.168.1.1:80");

  free(s);
  dpl_addrlist_free(addrlist);
}
END_TEST

START_TEST(multiple_add_test)
{
  dpl_addrlist_t *addrlist;
  dpl_status_t r;

  /* create the addrlist */
  addrlist = dpl_addrlist_create(NULL);
  dpl_assert_ptr_not_null(addrlist);

  /* add an address */
  r = dpl_addrlist_add_from_str(addrlist, "192.168.1.1:80");
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_int_eq(1, dpl_addrlist_count(addrlist));

  /* adding the same address again is a no-op (well not
   * quite but close enough). */
  r = dpl_addrlist_add_from_str(addrlist, "192.168.1.1:80");
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_int_eq(1, dpl_addrlist_count(addrlist));

  /* an address which is the same IP but a different
   * port counts as a different address */
  r = dpl_addrlist_add_from_str(addrlist, "192.168.1.1:8080");
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_int_eq(2, dpl_addrlist_count(addrlist));

  dpl_addrlist_free(addrlist);
}
END_TEST

START_TEST(create_from_str_1_test)
{
  dpl_addrlist_t *addrlist;
  char *hoststr = NULL;
  char *portstr = NULL;
  struct in_addr ipaddr;
  u_short port;
  dpl_status_t r;
  char *s;

  /* create the addrlist */
  addrlist = dpl_addrlist_create_from_str("4244", "192.168.1.1");
  dpl_assert_ptr_not_null(addrlist);

  /* verify length */
  dpl_assert_int_eq(1, dpl_addrlist_count(addrlist));

  /* verify getting elements by index */

  hoststr = portstr = NULL;
  memset(&ipaddr, 0, sizeof(ipaddr));
  port = 0;
  r = dpl_addrlist_get_nth(addrlist, 0, &hoststr, &portstr, &ipaddr, &port);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_str_eq(hoststr, "192.168.1.1");
  dpl_assert_str_eq(portstr, "4244");
  dpl_assert_str_eq(inet_ntoa(ipaddr), "192.168.1.1");
  dpl_assert_int_eq(port, 4244);

  /* indexes wrap modulo the count */

  hoststr = portstr = NULL;
  memset(&ipaddr, 0, sizeof(ipaddr));
  port = 0;
  r = dpl_addrlist_get_nth(addrlist, 1, &hoststr, &portstr, &ipaddr, &port);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_str_eq(hoststr, "192.168.1.1");
  dpl_assert_str_eq(portstr, "4244");
  dpl_assert_str_eq(inet_ntoa(ipaddr), "192.168.1.1");
  dpl_assert_int_eq(port, 4244);


  hoststr = portstr = NULL;
  memset(&ipaddr, 0, sizeof(ipaddr));
  port = 0;
  r = dpl_addrlist_get_nth(addrlist, 347, &hoststr, &portstr, &ipaddr, &port);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_str_eq(hoststr, "192.168.1.1");
  dpl_assert_str_eq(portstr, "4244");
  dpl_assert_str_eq(inet_ntoa(ipaddr), "192.168.1.1");
  dpl_assert_int_eq(port, 4244);

  /* verify the string form of the addrlist */
  s = dpl_addrlist_get(addrlist);
  dpl_assert_str_eq(s, "192.168.1.1:4244");

  free(s);
  dpl_addrlist_free(addrlist);
}
END_TEST

START_TEST(create_from_str_3_test)
{
  dpl_addrlist_t *addrlist;
  char *hoststr = NULL;
  char *portstr = NULL;
  struct in_addr ipaddr;
  u_short port;
  dpl_status_t r;
  int i;
  char *s;
  int port_position[3] = { 0, 0, 0 };
  int port_counts[3] = { 0, 0, 0 };
  char expstr[256] = "";

  /* create the addrlist */
  addrlist = dpl_addrlist_create_from_str("4244",
					  "192.168.1.1:4242,192.168.1.2:4243,192.168.1.3");
  dpl_assert_ptr_not_null(addrlist);

  /* verify length */
  dpl_assert_int_eq(3, dpl_addrlist_count(addrlist));

  /* verify getting elements by index */
  /* indexes wrap modulo the count */

  for (i = 0 ; i < 12 ; i++)
    {
      hoststr = portstr = NULL;
      memset(&ipaddr, 0, sizeof(ipaddr));
      port = 0;
      r = dpl_addrlist_get_nth(addrlist, i, &hoststr, &portstr, &ipaddr, &port);
      dpl_assert_int_eq(DPL_SUCCESS, r);

      fail_unless(port >= 4242, NULL);
      fail_unless(port <= 4244, NULL);
      if (port == 4242)
	{
	  dpl_assert_str_eq(hoststr, "192.168.1.1");
	  dpl_assert_str_eq(portstr, "4242");
	  dpl_assert_str_eq(inet_ntoa(ipaddr), "192.168.1.1");
	  port_counts[0]++;
	}
      else if (port == 4243)
	{
	  dpl_assert_str_eq(hoststr, "192.168.1.2");
	  dpl_assert_str_eq(portstr, "4243");
	  dpl_assert_str_eq(inet_ntoa(ipaddr), "192.168.1.2");
	  port_counts[1]++;
	}
      else /* 4244 */
	{
	  dpl_assert_str_eq(hoststr, "192.168.1.3");
	  dpl_assert_str_eq(portstr, "4244");
	  dpl_assert_str_eq(inet_ntoa(ipaddr), "192.168.1.3");
	  port_counts[2]++;
	}

      if (port_position[i % 3])
	dpl_assert_int_eq(port_position[i % 3], port);
      else
	port_position[i % 3] = port;
    }
  dpl_assert_int_eq(port_counts[0], 4);
  dpl_assert_int_eq(port_counts[1], 4);
  dpl_assert_int_eq(port_counts[2], 4);

  /* verify the string form of the addrlist */
  /* this is tough because the list is randomised internally */

  for (i = 0 ; i < 3 ; i++)
    {
      if (i)
	strcat(expstr, ",");
      if (port_position[i] == 4242)
	strcat(expstr, "192.168.1.1:4242");
      else if (port_position[i] == 4243)
	strcat(expstr, "192.168.1.2:4243");
      else if (port_position[i] == 4244)
	strcat(expstr, "192.168.1.3:4244");
    }

  s = dpl_addrlist_get(addrlist);
  dpl_assert_str_eq(s, expstr);

  free(s);
  dpl_addrlist_free(addrlist);
}
END_TEST

/* Return a "host:port" pair for the i'th address in the addrlist */
static char *
get_nth(dpl_addrlist_t *addrlist, int i)
{
  dpl_status_t r;
  char *host = NULL;
  char *port = NULL;
  char *addr;

  r = dpl_addrlist_get_nth(addrlist, i, &host, &port, NULL, NULL);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  addr = malloc(strlen(host) + 1 + strlen(port) + 1);
  dpl_assert_ptr_not_null(addr);
  strcpy(addr, host);
  strcat(addr, ":");
  strcat(addr, port);
  return addr;
}

static int
compare_addrs(const void *va, const void *vb)
{
  return strcmp(*(const char **)va, *(const char **)vb);
}

static char *
discover_traversible(dpl_addrlist_t *addrlist)
{
  int n;
  int i;
  int j;
  char *s;
  dpl_status_t r;
  char *addr;
#define MAXADDRS    32
  int naddrs = 0;
  char *addrs[MAXADDRS];

  n = dpl_addrlist_count(addrlist);
  for (i = 0 ; i < n ; i++)
    {
      addr = get_nth(addrlist, i);

      for (j = 0 ; j < naddrs ; j++)
	{
	  if (!strcmp(addr, addrs[j]))
	    break;
	}
      if (j == naddrs)
	{
	  dpl_assert_int_ne(naddrs, MAXADDRS);
	  addrs[naddrs++] = addr;
	}
      else
	free(addr);
    }

  /* sort the results into a predictable order */
  qsort(addrs, naddrs, sizeof(char*), compare_addrs);

  /* concatenate the strings into one big one */
  n = 0;
  for (i = 0 ; i < naddrs ; i++)
    n += strlen(addrs[i])+1;

  s = malloc(n);
  dpl_assert_ptr_not_null(s);
  s[0] = '\0';

  for (i = 0 ; i < naddrs ; i++)
    {
      if (i)
	strcat(s, ",");
      strcat(s, addrs[i]);
      free(addrs[i]);
    }

  return s;
}


START_TEST(blacklist_test)
{
  dpl_addrlist_t *addrlist;
  dpl_status_t r;
  const char *exp;
  char *act;
#define PORT "80"
#define ADDR1_H "192.168.1.1"
#define ADDR1 ADDR1_H":"PORT
#define ADDR2_H "192.168.1.2"
#define ADDR2 ADDR2_H":"PORT
#define ADDR3_H "192.168.1.3"
#define ADDR3 ADDR3_H":"PORT

  /* create the addrlist */
  addrlist = dpl_addrlist_create_from_str(NULL, ADDR1","ADDR2","ADDR3);
  dpl_assert_ptr_not_null(addrlist);
  dpl_assert_int_eq(3, dpl_addrlist_count(addrlist));
  exp = ADDR1","ADDR2","ADDR3;
  act = discover_traversible(addrlist);
  dpl_assert_str_eq(exp, act);
  free(act);

  /* attempting to blacklist an unknown address fails safely */
  r = dpl_addrlist_blacklist(addrlist, "192.168.1.32", PORT, 30/*seconds*/);
  dpl_assert_int_eq(DPL_ENOENT, r);
  dpl_assert_int_eq(3, dpl_addrlist_count(addrlist));
  exp = ADDR1","ADDR2","ADDR3;
  act = discover_traversible(addrlist);
  dpl_assert_str_eq(exp, act);
  free(act);

  /* blacklisting an address succeeds and makes the address untraversible */
  r = dpl_addrlist_blacklist(addrlist, ADDR2_H, PORT, 30/*seconds*/);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  /* note - count includes non-traversible addresses */
  dpl_assert_int_eq(3, dpl_addrlist_count(addrlist));
  act = discover_traversible(addrlist);
  exp = ADDR1","ADDR3;
  dpl_assert_str_eq(exp, act);
  free(act);

  /* blacklisting the same address again is a no-op (well not
   * quite but close enough). */
  r = dpl_addrlist_blacklist(addrlist, ADDR2_H, PORT, 30/*seconds*/);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_int_eq(3, dpl_addrlist_count(addrlist));
  act = discover_traversible(addrlist);
  exp = ADDR1","ADDR3;
  dpl_assert_str_eq(exp, act);
  free(act);

  /* un-blacklisting the address makes it traversible again */
  r = dpl_addrlist_unblacklist(addrlist, ADDR2_H, PORT);
  dpl_assert_int_eq(DPL_SUCCESS, r);
  dpl_assert_int_eq(3, dpl_addrlist_count(addrlist));
  act = discover_traversible(addrlist);
  exp = ADDR1","ADDR2","ADDR3;
  dpl_assert_str_eq(exp, act);
  free(act);

  /* attempting to un-blacklist an unknown address fails safely */
  r = dpl_addrlist_unblacklist(addrlist, "192.168.1.32", PORT);
  dpl_assert_int_eq(DPL_ENOENT, r);
  dpl_assert_int_eq(3, dpl_addrlist_count(addrlist));
  act = discover_traversible(addrlist);
  exp = ADDR1","ADDR2","ADDR3;
  dpl_assert_str_eq(exp, act);
  free(act);

  dpl_addrlist_free(addrlist);
#undef ADDR1_H
#undef ADDR1
#undef ADDR2_H
#undef ADDR2
#undef ADDR3_H
#undef ADDR3
#undef PORT
}
END_TEST

static uint64_t timeval_to_usec(const struct timeval *a)
{
  return a->tv_sec * 1000000 + a->tv_usec;
}

static uint64_t elapsed_usec(const struct timeval *start)
{
  struct timeval now;
  gettimeofday(&now, NULL);
  return timeval_to_usec(&now) - timeval_to_usec(start);
}

#define SECONDS		(1000000)	/* in microseconds */
#define MILLISECONDS	(1000)		/* in microseconds */

/*
 * Test that blacklisting is effective for a limited time period.
 * Note that <check.h> provides no facility for doing time warps
 * in the test process so we have to actually sleep.
 */
START_TEST(blacklist_timeout_test)
{
  dpl_addrlist_t *addrlist;
  unsigned long elapsed;
  dpl_status_t r;
  const char *exp;
  char *act;
  struct timeval start;
#define PORT "80"
#define ADDR1_H "192.168.1.1"
#define ADDR1 ADDR1_H":"PORT
#define ADDR2_H "192.168.1.2"
#define ADDR2 ADDR2_H":"PORT
#define ADDR3_H "192.168.1.3"
#define ADDR3 ADDR3_H":"PORT

  /* create the addrlist */
  addrlist = dpl_addrlist_create_from_str(NULL, ADDR1","ADDR2","ADDR3);
  dpl_assert_ptr_not_null(addrlist);
  dpl_assert_int_eq(3, dpl_addrlist_count(addrlist));
  exp = ADDR1","ADDR2","ADDR3;
  act = discover_traversible(addrlist);
  dpl_assert_str_eq(exp, act);
  free(act);

  /* blacklisting an address succeeds */
  gettimeofday(&start, NULL);
  r = dpl_addrlist_blacklist(addrlist, ADDR2_H, PORT, 2/*seconds*/);
  dpl_assert_int_eq(DPL_SUCCESS, r);

  while (elapsed_usec(&start) < 1 * SECONDS)
    {
//       fprintf(stderr, "elapsed: %lu usec\n", elapsed_usec(&start)); fflush(stderr);
      /* address is not traversible before expiry */
      dpl_assert_int_eq(3, dpl_addrlist_count(addrlist));
      act = discover_traversible(addrlist);
      exp = ADDR1","ADDR3;
      dpl_assert_str_eq(exp, act);
      free(act);

      usleep(100 * MILLISECONDS);
    }

  /* sleep a little more to make sure we're over the expiry */
  usleep(1500 * MILLISECONDS);

  /* address is traversible again after expiry */
  dpl_assert_int_eq(3, dpl_addrlist_count(addrlist));
  act = discover_traversible(addrlist);
  exp = ADDR1","ADDR2","ADDR3;
  dpl_assert_str_eq(exp, act);
  free(act);

  dpl_addrlist_free(addrlist);
#undef ADDR1_H
#undef ADDR1
#undef ADDR2_H
#undef ADDR2
#undef ADDR3_H
#undef ADDR3
#undef PORT
}
END_TEST

/* Passing addrlist=null to various functions fails cleanly */
START_TEST(null_test)
{
  char *s;

  /* dpl_addrlist_get() has an odd return for this corner case, but whatever */
  s = dpl_addrlist_get(NULL);
  dpl_assert_str_eq(s, "");
  free(s);

  dpl_assert_int_eq(0, dpl_addrlist_count(NULL));
  dpl_addrlist_free(NULL);
  dpl_assert_int_eq(DPL_FAILURE, dpl_addrlist_blacklist(NULL, "192.168.1.32", "80", 30));
  dpl_assert_int_eq(DPL_FAILURE, dpl_addrlist_unblacklist(NULL, "192.168.1.32", "80"));
  dpl_assert_int_eq(DPL_FAILURE, dpl_addrlist_set_from_str(NULL, "192.168.1.32"));
  dpl_assert_ptr_null(dpl_addrlist_create_from_str(NULL, NULL));
  dpl_addrlist_lock(NULL);
  dpl_addrlist_unlock(NULL);
  dpl_assert_int_eq(DPL_ENOENT, dpl_addrlist_get_nth(NULL, 1, &s, &s, NULL, NULL));
  dpl_assert_int_eq(DPL_FAILURE, dpl_addrlist_add(NULL, "192.168.1.32", "80"));
}
END_TEST


Suite *
addrlist_suite(void)
{
  Suite *s = suite_create("addrlist");
  TCase *t = tcase_create("base");
  tcase_add_test(t, empty_test);
  tcase_add_test(t, default_default_port_test);
  tcase_add_test(t, multiple_add_test);
  tcase_add_test(t, create_from_str_1_test);
  tcase_add_test(t, create_from_str_3_test);
  tcase_add_test(t, blacklist_test);
  tcase_add_test(t, blacklist_timeout_test);
  tcase_add_test(t, null_test);
  suite_add_tcase(s, t);
  return s;
}

