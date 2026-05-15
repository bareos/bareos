/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include "gtest/gtest.h"

#include "tools/setup_service_discovery.h"

using setup_service_discovery::DiscoverSetupService;
using setup_service_discovery::DeriveDiscoveryDomainFromFqdn;
using setup_service_discovery::DiscoveredEndpoint;
using setup_service_discovery::SrvRecord;

TEST(FdSetupDiscovery, DerivesDomainFromFqdn)
{
  EXPECT_EQ(DeriveDiscoveryDomainFromFqdn("client1.example.com"), "example.com");
}

TEST(FdSetupDiscovery, PicksLowestPrioritySrvRecord)
{
  const auto endpoint = DiscoverSetupService(
      "example.com", std::nullopt,
      [](const std::string& query_name) {
        EXPECT_EQ(query_name, "_bareos-setup._tcp.example.com");
        return std::vector<SrvRecord>{
            {.priority = 20, .weight = 1, .port = 5555, .target = "late.example.com"},
            {.priority = 10, .weight = 1, .port = 10443, .target = "early.example.com"}};
      },
      [](uint16_t upper_bound) {
        EXPECT_EQ(upper_bound, 1);
        return static_cast<uint16_t>(0);
      });

  EXPECT_EQ(endpoint.query_name, "_bareos-setup._tcp.example.com");
  EXPECT_EQ(endpoint.target, "early.example.com");
  EXPECT_EQ(endpoint.port, 10443);
}

TEST(FdSetupDiscovery, RespectsExplicitPortOverride)
{
  const auto endpoint = DiscoverSetupService(
      "example.com", 12345,
      [](const std::string&) {
        return std::vector<SrvRecord>{
            {.priority = 10, .weight = 5, .port = 10443, .target = "setup.example.com"}};
      },
      [](uint16_t upper_bound) {
        EXPECT_EQ(upper_bound, 5);
        return static_cast<uint16_t>(2);
      });

  EXPECT_EQ(endpoint.target, "setup.example.com");
  EXPECT_EQ(endpoint.port, 12345);
}
