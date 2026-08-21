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

#ifndef BAREOS_TOOLS_SETUP_SERVICE_DISCOVERY_H_
#define BAREOS_TOOLS_SETUP_SERVICE_DISCOVERY_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace setup_service_discovery {

struct SrvRecord {
  uint16_t priority{0};
  uint16_t weight{0};
  uint16_t port{0};
  std::string target;
};

struct DiscoveredEndpoint {
  std::string query_name;
  std::string target;
  uint16_t port{0};
};

using LookupSrvRecordsFn = std::function<std::vector<SrvRecord>(const std::string&)>;
using RandomBelowFn = std::function<uint16_t(uint16_t)>;

std::string DeriveDiscoveryDomainFromFqdn(const std::string& fqdn);
std::string DeriveLocalDiscoveryDomain();
std::vector<SrvRecord> LookupSrvRecords(const std::string& query_name);
uint16_t DefaultRandomBelow(uint16_t exclusive_upper_bound);
DiscoveredEndpoint DiscoverSetupService(
    const std::string& domain,
    std::optional<uint16_t> explicit_port = std::nullopt,
    LookupSrvRecordsFn lookup_fn = LookupSrvRecords,
    RandomBelowFn random_fn = DefaultRandomBelow);

}  // namespace setup_service_discovery

#endif  // BAREOS_TOOLS_SETUP_SERVICE_DISCOVERY_H_
