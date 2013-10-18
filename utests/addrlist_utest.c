#include <droplet.h>
#include <check.h>
#include <arpa/inet.h>
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
  fail_if(NULL == addrlist, NULL);

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
  fail_if(NULL == addrlist, NULL);

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
  fail_if(NULL == addrlist, NULL);

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


Suite *
addrlist_suite(void)
{
  Suite *s = suite_create("addrlist");
  TCase *t = tcase_create("base");
  tcase_add_test(t, empty_test);
  tcase_add_test(t, create_from_str_1_test);
  tcase_add_test(t, create_from_str_3_test);
  suite_add_tcase(s, t);
  return s;
}

