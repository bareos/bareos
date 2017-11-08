/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2007 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Written by Meno Abels, June MMIV
 */
/**
 * @file
 * address configuration
 */

class DLL_IMP_EXP IPADDR : public SMARTALLOC {
 public:
   typedef enum { R_SINGLE, R_SINGLE_PORT, R_SINGLE_ADDR, R_MULTIPLE,
                  R_DEFAULT, R_EMPTY
   } i_type;
   IPADDR(int af);
   IPADDR(const IPADDR & src);
 private:
   IPADDR() {  /* block this construction */ }
   i_type type;
   union {
      struct sockaddr dontuse;
      struct sockaddr_in dontuse4;
#ifdef HAVE_IPV6
      struct sockaddr_in6 dontuse6;
#endif
   } saddrbuf;
   struct sockaddr *saddr;
   struct sockaddr_in *saddr4;
#ifdef HAVE_IPV6
   struct sockaddr_in6 *saddr6;
#endif
 public:
   void set_type(i_type o);
   i_type get_type() const;
   unsigned short get_port_net_order() const;
   unsigned short get_port_host_order() const
   {
      return ntohs(get_port_net_order());
   }
   void set_port_net(unsigned short port);
   int get_family() const;
   struct sockaddr *get_sockaddr();
   int get_sockaddr_len();
   void copy_addr(IPADDR * src);
   void set_addr_any();
   void set_addr4(struct in_addr *ip4);
#ifdef HAVE_IPV6
   void set_addr6(struct in6_addr *ip6);
#endif
   const char *get_address(char *outputbuf, int outlen);
   const char *build_config_str(char *buf, int blen);
   const char *build_address_str(char *buf, int blen, bool print_port=true);

   /* private */
   dlink link;
};

DLL_IMP_EXP void init_default_addresses(dlist ** addr, const char *port);
DLL_IMP_EXP void free_addresses(dlist * addrs);

const char *get_first_address(dlist * addrs, char *outputbuf, int outlen);
int get_first_port_net_order(dlist * addrs);
int get_first_port_host_order(dlist * addrs);

DLL_IMP_EXP int add_address(dlist **out, IPADDR::i_type type, unsigned short defaultport, int family,
                const char *hostname_str, const char *port_str, char *buf, int buflen);
const char *build_addresses_str(dlist *addrs, char *buf, int blen, bool print_port=true);

int sockaddr_get_port_net_order(const struct sockaddr *sa);
int sockaddr_get_port(const struct sockaddr *sa);
char *sockaddr_to_ascii(const struct sockaddr *sa, char *buf, int len);
#ifdef WIN32
#undef HAVE_OLD_SOCKOPT
#endif
#ifdef HAVE_OLD_SOCKOPT
int inet_aton(const char *cp, struct in_addr *inp);
#endif
