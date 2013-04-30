/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2007 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *
 *   Written by Meno Abels, June MMIV
 *
 *   Version $Id$
 */


class IPADDR : public SMARTALLOC {
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

   const char *build_address_str(char *buf, int blen);

   /* private */
   dlink link;
};

extern void store_addresses(LEX * lc, RES_ITEM * item, int index, int pass);
extern void free_addresses(dlist * addrs);
extern void store_addresses_address(LEX * lc, RES_ITEM * item, int index, int pass);
extern void store_addresses_port(LEX * lc, RES_ITEM * item, int index, int pass);
extern void init_default_addresses(dlist ** addr, int port);

extern const char *get_first_address(dlist * addrs, char *outputbuf, int outlen);
extern int get_first_port_net_order(dlist * addrs);
extern int get_first_port_host_order(dlist * addrs);

extern const char *build_addresses_str(dlist *addrs, char *buf, int blen);

extern int sockaddr_get_port_net_order(const struct sockaddr *sa);
extern int sockaddr_get_port(const struct sockaddr *sa);
extern char *sockaddr_to_ascii(const struct sockaddr *sa, char *buf, int len);
#ifdef WIN32
#undef HAVE_OLD_SOCKOPT
#endif
#ifdef HAVE_OLD_SOCKOPT
extern int inet_aton(const char *cp, struct in_addr *inp);
#endif
