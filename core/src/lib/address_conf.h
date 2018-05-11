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

class DLL_IMP_EXP IPADDR : public SmartAlloc {
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
   void SetType(i_type o);
   i_type GetType() const;
   unsigned short GetPortNetOrder() const;
   unsigned short GetPortHostOrder() const
   {
      return ntohs(GetPortNetOrder());
   }
   void SetPortNet(unsigned short port);
   int GetFamily() const;
   struct sockaddr *get_sockaddr();
   int GetSockaddrLen();
   void CopyAddr(IPADDR * src);
   void SetAddrAny();
   void SetAddr4(struct in_addr *ip4);
#ifdef HAVE_IPV6
   void SetAddr6(struct in6_addr *ip6);
#endif
   const char *GetAddress(char *outputbuf, int outlen);
   const char *BuildConfigString(char *buf, int blen);
   const char *build_address_str(char *buf, int blen, bool print_port=true);

   /* private */
   dlink link;
};

DLL_IMP_EXP void InitDefaultAddresses(dlist ** addr, const char *port);
DLL_IMP_EXP void FreeAddresses(dlist * addrs);

DLL_IMP_EXP const char *GetFirstAddress(dlist * addrs, char *outputbuf, int outlen);
DLL_IMP_EXP int GetFirstPortNetOrder(dlist * addrs);
DLL_IMP_EXP int GetFirstPortHostOrder(dlist * addrs);

DLL_IMP_EXP int AddAddress(dlist **out, IPADDR::i_type type, unsigned short defaultport, int family,
                const char *hostname_str, const char *port_str, char *buf, int buflen);
DLL_IMP_EXP const char *BuildAddressesString(dlist *addrs, char *buf, int blen, bool print_port=true);

DLL_IMP_EXP int SockaddrGetPortNetOrder(const struct sockaddr *sa);
DLL_IMP_EXP int SockaddrGetPort(const struct sockaddr *sa);
DLL_IMP_EXP char *SockaddrToAscii(const struct sockaddr *sa, char *buf, int len);
#ifdef WIN32
#undef HAVE_OLD_SOCKOPT
#endif
#ifdef HAVE_OLD_SOCKOPT
DLL_IMP_EXP int inet_aton(const char *cp, struct in_addr *inp);
#endif
