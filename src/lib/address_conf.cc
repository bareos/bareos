/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Configuration file parser for IP-Addresse ipv4 and ipv6
 */

#include "bareos.h"
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
//#include <resolv.h>
#endif

IPADDR::IPADDR(const IPADDR &src) : type(src.type)
{
  memcpy(&saddrbuf, &src.saddrbuf, sizeof(saddrbuf));
  saddr  = &saddrbuf.dontuse;
  saddr4 = &saddrbuf.dontuse4;
#ifdef HAVE_IPV6
  saddr6 = &saddrbuf.dontuse6;
#endif
}

IPADDR::IPADDR(int af) : type(R_EMPTY)
{
#ifdef HAVE_IPV6
   if (!(af == AF_INET6 || af == AF_INET)) {
      Emsg1(M_ERROR_TERM, 0, _("Only ipv4 and ipv6 are supported (%d)\n"), af);
   }
#else
   if (af != AF_INET) {
      Emsg1(M_ERROR_TERM, 0, _("Only ipv4 is supported (%d)\n"), af);
   }
#endif

   memset(&saddrbuf, 0, sizeof(saddrbuf));
   saddr  = &saddrbuf.dontuse;
   saddr4 = &saddrbuf.dontuse4;
#ifdef HAVE_IPV6
   saddr6 = &saddrbuf.dontuse6;
#endif
   saddr->sa_family = af;
   switch (af) {
   case AF_INET:
      saddr4->sin_port = 0xffff;
#ifdef HAVE_SA_LEN
      saddr->sa_len = sizeof(sockaddr_in);
#endif
      break;
#ifdef HAVE_IPV6
   case AF_INET6:
      saddr6->sin6_port = 0xffff;
#ifdef HAVE_SA_LEN
      saddr->sa_len = sizeof(sockaddr_in6);
#endif
      break;
#endif
   }

   set_addr_any();
}

void IPADDR::set_type(i_type o)
{
   type = o;
}

IPADDR::i_type IPADDR::get_type() const
{
   return type;
}

unsigned short IPADDR::get_port_net_order() const
{
   unsigned short port = 0;
   if (saddr->sa_family == AF_INET) {
      port = saddr4->sin_port;
   }
#ifdef HAVE_IPV6
   else {
      port = saddr6->sin6_port;
   }
#endif
    return port;
}

void IPADDR::set_port_net(unsigned short port)
{
   if (saddr->sa_family == AF_INET) {
      saddr4->sin_port = port;
   }
#ifdef HAVE_IPV6
   else {
      saddr6->sin6_port = port;
   }
#endif
}

int IPADDR::get_family() const
{
    return saddr->sa_family;
}

struct sockaddr *IPADDR::get_sockaddr()
{
   return saddr;
}

int IPADDR::get_sockaddr_len()
{
#ifdef HAVE_IPV6
   return saddr->sa_family == AF_INET ? sizeof(*saddr4) : sizeof(*saddr6);
#else
   return sizeof(*saddr4);
#endif
}
void IPADDR::copy_addr(IPADDR *src)
{
   if (saddr->sa_family == AF_INET) {
      saddr4->sin_addr.s_addr = src->saddr4->sin_addr.s_addr;
   }
#ifdef HAVE_IPV6
   else {
      saddr6->sin6_addr = src->saddr6->sin6_addr;
   }
#endif
}

void IPADDR::set_addr_any()
{
   if (saddr->sa_family == AF_INET) {
      saddr4->sin_addr.s_addr = INADDR_ANY;
   }
#ifdef HAVE_IPV6
   else {
     saddr6->sin6_addr= in6addr_any;
   }
#endif
}

void IPADDR::set_addr4(struct in_addr *ip4)
{
   if (saddr->sa_family != AF_INET) {
      Emsg1(M_ERROR_TERM, 0, _("It was tried to assign a ipv6 address to a ipv4(%d)\n"), saddr->sa_family);
   }
   saddr4->sin_addr = *ip4;
}

#ifdef HAVE_IPV6
void IPADDR::set_addr6(struct in6_addr *ip6)
{
   if (saddr->sa_family != AF_INET6) {
      Emsg1(M_ERROR_TERM, 0, _("It was tried to assign a ipv4 address to a ipv6(%d)\n"), saddr->sa_family);
   }
   saddr6->sin6_addr = *ip6;
}
#endif

const char *IPADDR::get_address(char *outputbuf, int outlen)
{
   outputbuf[0] = '\0';
#ifdef HAVE_INET_NTOP
# ifdef HAVE_IPV6
   inet_ntop(saddr->sa_family, saddr->sa_family == AF_INET ?
              (void*)&(saddr4->sin_addr) : (void*)&(saddr6->sin6_addr),
              outputbuf, outlen);
# else
   inet_ntop(saddr->sa_family, (void*)&(saddr4->sin_addr), outputbuf, outlen);
# endif
#else
   bstrncpy(outputbuf, inet_ntoa(saddr4->sin_addr), outlen);
#endif
   return outputbuf;
}

const char *IPADDR::build_config_str(char *buf, int blen)
{
   char tmp[1024];

   switch (get_family()) {
   case AF_INET:
      bsnprintf(buf, blen, "      ipv4 = {\n"
                           "         addr = %s\n"
                           "         port = %hu\n"
                           "      }",
                get_address(tmp, sizeof(tmp) - 1),
                get_port_host_order());
      break;
   case AF_INET6:
      bsnprintf(buf, blen, "      ipv6 = {\n"
                           "         addr = %s\n"
                           "         port = %hu\n"
                           "      }",
                get_address(tmp, sizeof(tmp) - 1),
                get_port_host_order());
      break;
   default:
      break;
   }

   return buf;
}

const char *IPADDR::build_address_str(char *buf, int blen, bool print_port/*=true*/)
{
   char tmp[1024];
   if (print_port) {
      switch (get_family()) {
      case AF_INET:
         bsnprintf(buf, blen, "host[ipv4;%s;%hu] ",
                   get_address(tmp, sizeof(tmp) - 1), get_port_host_order());
         break;
      case AF_INET6:
         bsnprintf(buf, blen, "host[ipv6;%s;%hu] ",
                   get_address(tmp, sizeof(tmp) - 1), get_port_host_order());
         break;
      default:
         break;
      }
   } else {
      switch (get_family()) {
      case AF_INET:
         bsnprintf(buf, blen, "host[ipv4;%s] ",
                   get_address(tmp, sizeof(tmp) - 1));
         break;
      case AF_INET6:
         bsnprintf(buf, blen, "host[ipv6;%s] ",
                   get_address(tmp, sizeof(tmp) - 1));
         break;
      default:
         break;
      }
   }

   return buf;
}

const char *build_addresses_str(dlist *addrs, char *buf, int blen, bool print_port/*=true*/)
{
   if (!addrs || addrs->size() == 0) {
      bstrncpy(buf, "", blen);
      return buf;
   }
   char *work = buf;
   IPADDR *p;
   foreach_dlist(p, addrs) {
      char tmp[1024];
      int len = bsnprintf(work, blen, "%s", p->build_address_str(tmp, sizeof(tmp), print_port));
      if (len < 0)
         break;
      work += len;
      blen -= len;
   }
   return buf;
}

const char *get_first_address(dlist *addrs, char *outputbuf, int outlen)
{
   return ((IPADDR *)(addrs->first()))->get_address(outputbuf, outlen);
}

int get_first_port_net_order(dlist *addrs)
{
   if (!addrs) {
      return 0;
   } else {
      return ((IPADDR *)(addrs->first()))->get_port_net_order();
   }
}

int get_first_port_host_order(dlist *addrs)
{
   if (!addrs) {
      return 0;
   } else {
      return ((IPADDR *)(addrs->first()))->get_port_host_order();
   }
}

int add_address(dlist **out, IPADDR::i_type type, unsigned short defaultport, int family,
                const char *hostname_str, const char *port_str, char *buf, int buflen)
{
   IPADDR *iaddr;
   IPADDR *jaddr;
   dlist *hostaddrs;
   unsigned short port;
   IPADDR::i_type intype = type;

   buf[0] = 0;
   dlist *addrs = (dlist *)(*(out));
   if (!addrs) {
      IPADDR *tmp = 0;
      addrs = *out = New(dlist(tmp, &tmp->link));
   }

   type = (type == IPADDR::R_SINGLE_PORT
           || type == IPADDR::R_SINGLE_ADDR) ? IPADDR::R_SINGLE : type;
   if (type != IPADDR::R_DEFAULT) {
      IPADDR *def = 0;
      foreach_dlist(iaddr, addrs) {
         if (iaddr->get_type() == IPADDR::R_DEFAULT) {
            def = iaddr;
         } else if (iaddr->get_type() != type) {
            bsnprintf(buf, buflen,
                      _("the old style addresses cannot be mixed with new style"));
            return 0;
         }
      }
      if (def) {
         addrs->remove(def);
         delete def;
      }
   }

   if (!port_str || port_str[0] == '\0') {
      port = defaultport;
   } else {
      int pnum = atol(port_str);
      if (0 < pnum && pnum < 0xffff) {
         port = htons(pnum);
      } else {
         struct servent *s = getservbyname(port_str, "tcp");
         if (s) {
            port = s->s_port;
         } else {
            bsnprintf(buf, buflen, _("can't resolve service(%s)"), port_str);
            return 0;
         }
      }
   }

   const char *myerrstr;
   hostaddrs = bnet_host2ipaddrs(hostname_str, family, &myerrstr);
   if (!hostaddrs) {
      bsnprintf(buf, buflen, _("can't resolve hostname(%s) %s"), hostname_str,
                myerrstr);
      return 0;
   }

   if (intype == IPADDR::R_SINGLE_PORT || intype == IPADDR::R_SINGLE_ADDR) {
      IPADDR *addr;
      if (addrs->size()) {
         addr = (IPADDR *)addrs->first();
      } else {
         addr = New(IPADDR(family));
         addr->set_type(type);
         addr->set_port_net(defaultport);
         addr->set_addr_any();
         addrs->append(addr);
      }
      if (intype == IPADDR::R_SINGLE_PORT) {
         addr->set_port_net(port);
      }
      if (intype == IPADDR::R_SINGLE_ADDR) {
         addr->copy_addr((IPADDR *)(hostaddrs->first()));
      }
   } else {
      foreach_dlist(iaddr, hostaddrs) {
         IPADDR *clone;
         /* for duplicates */
         foreach_dlist(jaddr, addrs) {
            if (iaddr->get_sockaddr_len() == jaddr->get_sockaddr_len() &&
            !memcmp(iaddr->get_sockaddr(), jaddr->get_sockaddr(),
                    iaddr->get_sockaddr_len()))
                {
               goto skip;          /* no price */
            }
         }
         clone = New(IPADDR(*iaddr));
         clone->set_type(type);
         clone->set_port_net(port);
         addrs->append(clone);
       skip:
         continue;
      }
   }
   free_addresses(hostaddrs);
   return 1;
}

void init_default_addresses(dlist **out, const char *port)
{
   char buf[1024];
   unsigned short sport = str_to_int32(port);

   if (!add_address(out, IPADDR::R_DEFAULT, htons(sport), AF_INET, 0, 0, buf, sizeof(buf))) {
      Emsg1(M_ERROR_TERM, 0, _("Can't add default address (%s)\n"), buf);
   }
}

void free_addresses(dlist * addrs)
{
   while (!addrs->empty()) {
      IPADDR *ptr = (IPADDR*)addrs->first();
      addrs->remove(ptr);
      delete ptr;
   }
   delete addrs;
}

int sockaddr_get_port_net_order(const struct sockaddr *client_addr)
{
   if (client_addr->sa_family == AF_INET) {
      return ((struct sockaddr_in *)client_addr)->sin_port;
   }
#ifdef HAVE_IPV6
   else {
      return ((struct sockaddr_in6 *)client_addr)->sin6_port;
   }
#endif
   return -1;
}

int sockaddr_get_port(const struct sockaddr *client_addr)
{
   if (client_addr->sa_family == AF_INET) {
      return ntohs(((struct sockaddr_in *)client_addr)->sin_port);
   }
#ifdef HAVE_IPV6
   else {
      return ntohs(((struct sockaddr_in6 *)client_addr)->sin6_port);
   }
#endif
   return -1;
}

char *sockaddr_to_ascii(const struct sockaddr *sa, char *buf, int len)
{
#ifdef HAVE_INET_NTOP
   /* MA Bug 5 the problem was that i mixed up sockaddr and in_addr */
   inet_ntop(sa->sa_family,
# ifdef HAVE_IPV6
             sa->sa_family == AF_INET ?
                 (void*)&(((struct sockaddr_in*)sa)->sin_addr) :
                 (void*)&(((struct sockaddr_in6*)sa)->sin6_addr),
# else
                 (void*)&(((struct sockaddr_in*)sa)->sin_addr),
# endif /* HAVE_IPV6 */
             buf, len);
#else
   bstrncpy(buf, inet_ntoa(((struct sockaddr_in *)sa)->sin_addr), len);
#endif
   return buf;
}

#ifdef HAVE_OLD_SOCKOPT
int inet_aton(const char *cp, struct in_addr *inp)
{
   struct in_addr inaddr;

   if((inaddr.s_addr = inet_addr(cp)) != INADDR_NONE) {
      inp->s_addr = inaddr.s_addr;
      return 1;
   }
   return 0;
}
#endif
