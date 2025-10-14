/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
// Written by Meno Abels, June MMIV
/**
 * @file
 * Configuration file parser for IP-Addresse ipv4 and ipv6
 */
#include <netdb.h>
#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif

#include "include/bareos.h"
#include "lib/address_conf.h"
#include "lib/bnet.h"
#include "lib/bsys.h"
#include "lib/edit.h"
#include "lib/output_formatter_resource.h"
#include "lib/berrno.h"

#if !defined(HAVE_MSVC)
#  define socketClose(...) close(__VA_ARGS__)
#else
#  define socketClose(...) closesocket(__VA_ARGS__)
#endif


#if __has_include(<arpa/nameser.h>)
#  include <arpa/nameser.h>
#endif

IPADDR::IPADDR(const IPADDR& src)
{
  type = src.type;
  memcpy(&addr_storage, &src.addr_storage, sizeof(addr_storage));
}

IPADDR::IPADDR(int af)
{
  type = R_EMPTY;
  if (af != AF_INET6 && af != AF_INET) {
    Emsg1(M_ERROR_TERM, 0, T_("Only ipv4 and ipv6 are supported (%d)\n"), af);
  }

  switch (af) {
    case AF_INET: {
      addr_in.sin_family = af;
      addr_in.sin_port = 0xffff;
    } break;
    case AF_INET6: {
      addr_in6.sin6_family = af;
      addr_in6.sin6_port = 0xffff;
    } break;
  }

  SetAddrAny();
}

void IPADDR::SetType(i_type o) { type = o; }

IPADDR::i_type IPADDR::GetType() const { return type; }

unsigned short IPADDR::GetPortNetOrder() const
{
  unsigned short port = 0;
  if (addr.sa_family == AF_INET) {
    port = addr_in.sin_port;
  } else {
    port = addr_in6.sin6_port;
  }
  return port;
}

void IPADDR::SetPortNet(unsigned short port)
{
  if (addr.sa_family == AF_INET) {
    addr_in.sin_port = port;
  } else {
    addr_in6.sin6_port = port;
  }
}

int IPADDR::GetFamily() const { return addr.sa_family; }

struct sockaddr* IPADDR::get_sockaddr() { return &addr; }

int IPADDR::GetSockaddrLen()
{
  return addr.sa_family == AF_INET ? sizeof(addr_in) : sizeof(addr_in6);
}
void IPADDR::CopyAddr(IPADDR* src)
{
  if (addr.sa_family == AF_INET) {
    addr_in.sin_addr.s_addr = src->addr_in.sin_addr.s_addr;
  } else {
    addr_in6.sin6_addr = src->addr_in6.sin6_addr;
  }
}

void IPADDR::SetAddrAny()
{
  if (addr.sa_family == AF_INET) {
    addr_in.sin_addr.s_addr = INADDR_ANY;
  } else {
    addr_in6.sin6_addr = in6addr_any;
  }
}

void IPADDR::SetAddr4(struct in_addr* ip4)
{
  if (addr.sa_family != AF_INET) {
    Emsg1(M_ERROR_TERM, 0,
          T_("It was tried to assign a ipv6 address to a ipv4(%d)\n"),
          addr.sa_family);
  }
  addr_in.sin_addr = *ip4;
}

void IPADDR::SetAddr6(struct in6_addr* ip6)
{
  if (addr.sa_family != AF_INET6) {
    Emsg1(M_ERROR_TERM, 0,
          T_("It was tried to assign a ipv4 address to a ipv6(%d)\n"),
          addr.sa_family);
  }
  addr_in6.sin6_addr = *ip6;
}

const char* IPADDR::GetAddress(char* outputbuf, int outlen)
{
  outputbuf[0] = '\0';
  inet_ntop(addr.sa_family,
            addr.sa_family == AF_INET ? (void*)&(addr_in.sin_addr)
                                      : (void*)&(addr_in6.sin6_addr),
            outputbuf, outlen);
  return outputbuf;
}

void IPADDR::BuildConfigString(OutputFormatterResource& send, bool as_comment)
{
  char tmp[1024];
  std::string formatstring;

  switch (GetFamily()) {
    case AF_INET:
      send.SubResourceStart("ipv4", as_comment, "%s = {\n");
      send.KeyString("addr", GetAddress(tmp, sizeof(tmp) - 1), as_comment);
      send.KeyUnsignedInt("port", GetPortHostOrder(), as_comment);
      send.SubResourceEnd("ipv4", as_comment);
      break;
    case AF_INET6:
      send.SubResourceStart("ipv6", as_comment, "%s = {\n");
      send.KeyString("addr", GetAddress(tmp, sizeof(tmp) - 1), as_comment);
      send.KeyUnsignedInt("port", GetPortHostOrder(), as_comment);
      send.SubResourceEnd("ipv6", as_comment);
      break;
    default:
      break;
  }
}


const char* IPADDR::build_address_str(char* buf,
                                      int blen,
                                      bool print_port /*=true*/)
{
  char tmp[1024];
  if (print_port) {
    switch (GetFamily()) {
      case AF_INET:
        Bsnprintf(buf, blen, "host[ipv4;%s;%hu] ",
                  GetAddress(tmp, sizeof(tmp) - 1), GetPortHostOrder());
        break;
      case AF_INET6:
        Bsnprintf(buf, blen, "host[ipv6;%s;%hu] ",
                  GetAddress(tmp, sizeof(tmp) - 1), GetPortHostOrder());
        break;
      default:
        break;
    }
  } else {
    switch (GetFamily()) {
      case AF_INET:
        Bsnprintf(buf, blen, "host[ipv4;%s] ",
                  GetAddress(tmp, sizeof(tmp) - 1));
        break;
      case AF_INET6:
        Bsnprintf(buf, blen, "host[ipv6;%s] ",
                  GetAddress(tmp, sizeof(tmp) - 1));
        break;
      default:
        break;
    }
  }

  return buf;
}


// check if two addresses are the same
bool IsSameIpAddress(IPADDR* first, IPADDR* second)
{
  if (first == nullptr || second == nullptr) { return false; }
  return (first->GetSockaddrLen() == second->GetSockaddrLen()
          && memcmp(first->get_sockaddr(), second->get_sockaddr(),
                    first->GetSockaddrLen())
                 == 0);
}


const char* BuildAddressesString(dlist<IPADDR>* addrs,
                                 char* buf,
                                 int blen,
                                 bool print_port /*=true*/)
{
  if (!addrs || addrs->size() == 0) {
    bstrncpy(buf, "", blen);
    return buf;
  }
  char* work = buf;
  IPADDR* p;
  foreach_dlist (p, addrs) {
    char tmp[1024];
    int len = Bsnprintf(work, blen, "%s",
                        p->build_address_str(tmp, sizeof(tmp), print_port));
    if (len < 0) break;
    work += len;
    blen -= len;
  }
  return buf;
}

int GetFirstPortHostOrder(dlist<IPADDR>* addrs)
{
  if (!addrs) {
    return 0;
  } else {
    return ((IPADDR*)(addrs->first()))->GetPortHostOrder();
  }
}

bool RemoveDefaultAddresses(dlist<IPADDR>* addrs,
                            IPADDR::i_type type,
                            char* buf,
                            int buflen)
{
  IPADDR* iaddr;
  IPADDR* default_address = nullptr;
  IPADDR* todelete = nullptr;

  foreach_dlist (iaddr, addrs) {
    if (todelete) {
      delete (todelete);
      todelete = nullptr;
    }
    if (iaddr->GetType() == IPADDR::R_DEFAULT) {
      default_address = iaddr;
      if (default_address) {
        addrs->remove(default_address);
        todelete = default_address;
      }
    } else if (iaddr->GetType() != type) {
      Bsnprintf(buf, buflen,
                T_("the old style addresses cannot be mixed with new style"));
      return false;
    }
  }
  if (todelete) {
    delete (todelete);
    todelete = nullptr;
  }
  return true;
}

bool SetupPort(unsigned short& port,
               int defaultport,
               const char* port_str,
               char* buf,
               int buflen)
{
  if (!port_str || port_str[0] == '\0') {
    port = defaultport;
    return true;
  } else {
    int pnum = atol(port_str);
    if (0 < pnum && pnum < 0xffff) {
      port = htons(pnum);
      return true;
    } else {
      struct servent* s = getservbyname(port_str, "tcp");
      if (s) {
        port = s->s_port;
        return true;
      } else {
        Bsnprintf(buf, buflen, T_("can't resolve service(%s)"), port_str);
        return false;
      }
    }
  }
}

bool IsFamilyEnabled(IpFamily fam)
{
  static bool family_enabled[] = {CheckIfFamilyEnabled(IpFamily::V4),
                                  CheckIfFamilyEnabled(IpFamily::V6)};
  return family_enabled[static_cast<std::underlying_type_t<IpFamily>>(fam)];
}

std::optional<int> GetFamily(IpFamily family)
{
  switch (family) {
    case IpFamily::V4:
      return AF_INET;
    case IpFamily::V6:
      return AF_INET6;
    default:
      return std::nullopt;
  }
}

const char* FamilyName(IpFamily fam)
{
  switch (fam) {
    case IpFamily::V4:
      return "IPv4";
    case IpFamily::V6:
      return "IPv6";
    default:
      return "*Unknown Protocol*";
  }
}

bool CheckIfFamilyEnabled(IpFamily family)
{
  int tries = 0;
  int fd;
  do {
    ++tries;
    if ((fd = socket(GetFamily(family).value(), SOCK_STREAM, 0)) < 0) {
      Bmicrosleep(1, 0);
    }
  } while (fd < 0 && tries < 3);

  if (fd < 0) {
    BErrNo be;
    Emsg2(M_WARNING, 0, T_("Cannot open %s stream socket. ERR=%s\n"),
          FamilyName(family), be.bstrerror());
    return false;
  }
#ifdef HAVE_WIN32
  ::closesocket(fd);
#else
  ::close(fd);
#endif
  return true;
}

int AddAddress(dlist<IPADDR>** out,
               IPADDR::i_type type,
               unsigned short defaultport,
               int family,
               const char* hostname_str,
               const char* port_str,
               char* buf,
               int buflen)
{
  IPADDR* iaddr = nullptr;
  IPADDR* jaddr = nullptr;
  dlist<IPADDR>* hostaddrs = nullptr;
  unsigned short port;
  IPADDR::i_type intype = type;

  buf[0] = 0;
  dlist<IPADDR>* addrs = *(out);
  if (!addrs) { addrs = *out = new dlist<IPADDR>(); }

  type = (type == IPADDR::R_SINGLE_PORT || type == IPADDR::R_SINGLE_ADDR)
             ? IPADDR::R_SINGLE
             : type;

  if (type != IPADDR::R_DEFAULT) {
    if (!RemoveDefaultAddresses(addrs, type, buf, buflen)) { return 0; }
  }

  if (!SetupPort(port, defaultport, port_str, buf, buflen)) { return 0; }

  if (family == 0) {
    bool ipv4_enabled = IsFamilyEnabled(IpFamily::V4);
    bool ipv6_enabled = IsFamilyEnabled(IpFamily::V6);
    if (!ipv4_enabled && ipv6_enabled) { family = AF_INET6; }
    if (ipv4_enabled && !ipv6_enabled) { family = AF_INET; }
    if (!ipv4_enabled && !ipv6_enabled) {
      Bsnprintf(buf, buflen, T_("Both IPv4 are IPv6 are disabled!"));
      return 0;
    }
  } else if (family == AF_INET6 && !IsFamilyEnabled(IpFamily::V6)) {
    Bsnprintf(buf, buflen, T_("IPv6 address wanted but IPv6 is disabled!"));
    return 0;
  } else if (family == AF_INET && !IsFamilyEnabled(IpFamily::V4)) {
    Bsnprintf(buf, buflen, T_("IPv4 address wanted but IPv4 is disabled!"));
    return 0;
  }

  const char* myerrstr;
  hostaddrs = BnetHost2IpAddrs(hostname_str, family, &myerrstr);
  if (!hostaddrs) {
    Bsnprintf(buf, buflen, T_("can't resolve hostname(%s) %s"), hostname_str,
              myerrstr);
    return 0;
  }

  if (intype == IPADDR::R_SINGLE_PORT) {
    IPADDR* addr;
    if (addrs->size()) {
      addr = (IPADDR*)addrs->first();
    } else {
      addr = new IPADDR(family);
      addr->SetType(type);
      addr->SetPortNet(defaultport);
      addr->SetAddrAny();
      addrs->append(addr);
    }
    addr->SetPortNet(port);

  } else if (intype == IPADDR::R_SINGLE_ADDR) {
    IPADDR* addr = nullptr;
    int addr_port = defaultport;

    if (addrs->size()) {
      addr = (IPADDR*)addrs->first();
      addr_port = addr->GetPortNetOrder();
      EmptyAddressList(addrs);
    }

    addr = new IPADDR(family);
    addr->SetType(type);
    addr->SetPortNet(addr_port);
    addr->CopyAddr((IPADDR*)(hostaddrs->first()));
    addrs->append(addr);

  } else {
    foreach_dlist (iaddr, hostaddrs) {
      bool sameaddress = false;
      /* for duplicates */
      foreach_dlist (jaddr, addrs) {
        if (IsSameIpAddress(iaddr, jaddr)) {
          sameaddress = true;
          break;
        }
      }
      if (!sameaddress) {
        IPADDR* clone = nullptr;
        clone = new IPADDR(*iaddr);
        clone->SetType(type);
        clone->SetPortNet(port);
        addrs->append(clone);
      }
    }
  }
  FreeAddresses(hostaddrs);
  return 1;
}

void InitDefaultAddresses(dlist<IPADDR>** out, const char* port)
{
  char buf[1024];
  unsigned short sport = str_to_int32(port);

  bool ipv4_added = true;
  bool ipv6_added = true;

  if (!AddAddress(out, IPADDR::R_DEFAULT, htons(sport), AF_INET, 0, 0, buf,
                  sizeof(buf))) {
    Emsg1(M_WARNING, 0, T_("Can't add default IPv4 address (%s)\n"), buf);
    ipv4_added = false;
  }

  if (!AddAddress(out, IPADDR::R_DEFAULT, htons(sport), AF_INET6, 0, 0, buf,
                  sizeof(buf))) {
    Emsg1(M_WARNING, 0, T_("Can't add default IPv6 address (%s)\n"), buf);
    ipv6_added = false;
  }

  if (!ipv6_added && !ipv4_added) {
    Emsg0(M_ERROR_TERM, 0, T_("Can't add default IPv6 and IPv4 addresses\n"));
  }
}

void EmptyAddressList(dlist<IPADDR>* addrs)
{
  IPADDR* iaddr = nullptr;
  IPADDR* addrtodelete = nullptr;
  foreach_dlist (iaddr, addrs) {
    if (addrtodelete) {
      delete (addrtodelete);
      addrtodelete = nullptr;
    }
    if (iaddr) {
      addrtodelete = iaddr;
      addrs->remove(iaddr);
    }
  }
  if (addrtodelete) {
    delete (addrtodelete);
    addrtodelete = nullptr;
  }
}

void FreeAddresses(dlist<IPADDR>* addrs)
{
  while (!addrs->empty()) {
    IPADDR* ptr = (IPADDR*)addrs->first();
    addrs->remove(ptr);
    delete ptr;
  }
  delete addrs;
}

char* SockaddrToAscii(const struct sockaddr_storage* sa, char* buf, int len)
{
  auto* addr = reinterpret_cast<const sockaddr*>(sa);
  /* MA Bug 5 the problem was that i mixed up sockaddr and in_addr */
  inet_ntop(addr->sa_family,
            addr->sa_family == AF_INET
                ? (void*)&(((struct sockaddr_in*)sa)->sin_addr)
                : (void*)&(((struct sockaddr_in6*)sa)->sin6_addr),
            buf, len);
  return buf;
}

#ifdef HAVE_OLD_SOCKOPT
int inet_aton(const char* cp, struct in_addr* inp)
{
  struct in_addr inaddr;

  if ((inaddr.s_addr = InetAddr(cp)) != INADDR_NONE) {
    inp->s_addr = inaddr.s_addr;
    return 1;
  }
  return 0;
}
#endif
