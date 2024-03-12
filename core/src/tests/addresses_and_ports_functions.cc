/*
   BAREOS® - Backup Archiving REcovery Open Sourced

Copyright (C) 2023-2024 Bareos GmbH & Co. KG

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

#include "testing_dir_common.h"

#include "lib/address_conf.h"
#include "lib/bnet.h"

const unsigned short default_port = 9101;

std::vector<std::string> CreateAddressesFromAddAddress(
    IPADDR::i_type type,
    int family,
    unsigned short t_default_port,
    const char* hostname_str,
    const char* port_str)
{
  std::vector<std::string> newaddresses{};
  char buf[1024];
  dlist<IPADDR>* addresses = nullptr;

  AddAddress(&addresses, type, htons(t_default_port), family, hostname_str,
             port_str, buf, sizeof(buf));

  IPADDR* addr = nullptr;
  foreach_dlist (addr, addresses) {
    addr->build_address_str(buf, sizeof(buf), true);

    std::string theaddress(buf);
    theaddress.pop_back();

    newaddresses.emplace_back(std::move(theaddress));
  }
  FreeAddresses(addresses);
  return newaddresses;
}

class AddressesAndPortsFunctions : public ::testing::Test {
  void SetUp() override { InitDirGlobals(); }
};

TEST_F(AddressesAndPortsFunctions, DefaultAddressesAreCorrectlyProcessed)
{
  std::vector<std::string> addresses = CreateAddressesFromAddAddress(
      IPADDR::R_DEFAULT, 0, default_port, "", "");
  std::vector<std::string> expected_addresses{"host[ipv4;0.0.0.0;9101]",
                                              "host[ipv6;::;9101]"};
  EXPECT_EQ(addresses, expected_addresses);
}

TEST_F(AddressesAndPortsFunctions, IPv4AddressesAreCorrectlyProcessed)
{
  std::vector<std::string> addresses = CreateAddressesFromAddAddress(
      IPADDR::R_DEFAULT, AF_INET, default_port, "", "");
  std::vector<std::string> expected_addresses{"host[ipv4;0.0.0.0;9101]"};
  EXPECT_EQ(addresses, expected_addresses);

  addresses = CreateAddressesFromAddAddress(IPADDR::R_MULTIPLE, AF_INET,
                                            default_port, "1.2.3.4", "5000");
  expected_addresses = {"host[ipv4;1.2.3.4;5000]"};
  EXPECT_EQ(addresses, expected_addresses);

  addresses = CreateAddressesFromAddAddress(IPADDR::R_MULTIPLE, AF_INET,
                                            default_port, "257.2.3.4", "5000");
  EXPECT_TRUE(addresses.empty());

  addresses = CreateAddressesFromAddAddress(IPADDR::R_MULTIPLE, AF_INET,
                                            default_port, "1.2.3.4", "70000");
  EXPECT_TRUE(addresses.empty());
}

TEST_F(AddressesAndPortsFunctions, IPv6AddressesAreCorrectlyProcessed)
{
  std::vector<std::string> addresses = CreateAddressesFromAddAddress(
      IPADDR::R_DEFAULT, AF_INET6, default_port, "", "");
  std::vector<std::string> expected_addresses{"host[ipv6;::;9101]"};
  EXPECT_EQ(addresses, expected_addresses);

  addresses = CreateAddressesFromAddAddress(IPADDR::R_MULTIPLE, AF_INET6,
                                            default_port, "", "");
  expected_addresses = {"host[ipv6;::;9101]"};
  EXPECT_EQ(addresses, expected_addresses);

  addresses = CreateAddressesFromAddAddress(
      IPADDR::R_MULTIPLE, AF_INET6, default_port, "1:2:3:4:5:6:7:8", "5000");
  expected_addresses = {"host[ipv6;1:2:3:4:5:6:7:8;5000]"};
  EXPECT_EQ(addresses, expected_addresses);

  addresses = CreateAddressesFromAddAddress(IPADDR::R_MULTIPLE, AF_INET6,
                                            default_port, "AAAA:AAAA:AAAA", "");
  EXPECT_TRUE(addresses.empty());
}
