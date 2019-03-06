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

#include "lib/dlist.h"

class IPADDR : public SmartAlloc {
 public:
  typedef enum
  {
    R_SINGLE,
    R_SINGLE_PORT,
    R_SINGLE_ADDR,
    R_MULTIPLE,
    R_DEFAULT,
    R_EMPTY,
    R_UNDEFINED
  } i_type;
  IPADDR(int af);
  IPADDR(const IPADDR& src);

 private:
  IPADDR();
  i_type type;
  union {
    struct sockaddr dontuse;
    struct sockaddr_in dontuse4;
#ifdef HAVE_IPV6
    struct sockaddr_in6 dontuse6;
#endif
  } saddrbuf;
  struct sockaddr* saddr;
  struct sockaddr_in* saddr4;
#ifdef HAVE_IPV6
  struct sockaddr_in6* saddr6;
#endif
 public:
  void SetType(i_type o);
  i_type GetType() const;
  unsigned short GetPortNetOrder() const;
  unsigned short GetPortHostOrder() const { return ntohs(GetPortNetOrder()); }
  void SetPortNet(unsigned short port);
  int GetFamily() const;
  struct sockaddr* get_sockaddr();
  int GetSockaddrLen();
  void CopyAddr(IPADDR* src);
  void SetAddrAny();
  void SetAddr4(struct in_addr* ip4);
#ifdef HAVE_IPV6
  void SetAddr6(struct in6_addr* ip6);
#endif
  const char* GetAddress(char* outputbuf, int outlen);
  const char* BuildConfigString(char* buf, int blen);
  const char* build_address_str(char* buf, int blen, bool print_port = true);

  /* private */
  dlink link;
};

void InitDefaultAddresses(dlist** addr, const char* port);
void FreeAddresses(dlist* addrs);

const char* GetFirstAddress(dlist* addrs, char* outputbuf, int outlen);
int GetFirstPortNetOrder(dlist* addrs);
int GetFirstPortHostOrder(dlist* addrs);

int AddAddress(dlist** out,
               IPADDR::i_type type,
               unsigned short defaultport,
               int family,
               const char* hostname_str,
               const char* port_str,
               char* buf,
               int buflen);
const char* BuildAddressesString(dlist* addrs,
                                 char* buf,
                                 int blen,
                                 bool print_port = true);

int SockaddrGetPortNetOrder(const struct sockaddr* sa);
int SockaddrGetPort(const struct sockaddr* sa);
char* SockaddrToAscii(const struct sockaddr* sa, char* buf, int len);
#ifdef WIN32
#undef HAVE_OLD_SOCKOPT
#endif
#ifdef HAVE_OLD_SOCKOPT
int inet_aton(const char* cp, struct in_addr* inp);
#endif
