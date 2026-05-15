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

#include "tools/setup_service_discovery.h"

#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>

namespace setup_service_discovery {
namespace {

[[noreturn]] void Throw(const std::string& message)
{
  throw std::runtime_error(message);
}

bool IsUnsafeDerivedDomain(const std::string& domain)
{
  return domain.empty() || domain == "localhost" || domain == "localdomain"
         || domain == "localhost.localdomain";
}

}  // namespace

std::string DeriveDiscoveryDomainFromFqdn(const std::string& fqdn)
{
  const auto dot = fqdn.find('.');
  if (dot == std::string::npos || dot + 1 >= fqdn.size()) {
    Throw("Could not derive a discovery domain from the local host name."
          " Use --discovery-domain or --address.");
  }

  const auto domain = fqdn.substr(dot + 1);
  if (IsUnsafeDerivedDomain(domain)) {
    Throw("The local host name does not yield a usable discovery domain."
          " Use --discovery-domain or --address.");
  }
  return domain;
}

std::string DeriveLocalDiscoveryDomain()
{
  std::array<char, NI_MAXHOST> hostname{};
  if (gethostname(hostname.data(), hostname.size()) != 0) {
    Throw("Failed to read the local host name for setup-service discovery."
          " Use --discovery-domain or --address.");
  }
  hostname.back() = '\0';

  std::string host = hostname.data();
  if (host.find('.') != std::string::npos) {
    return DeriveDiscoveryDomainFromFqdn(host);
  }

  addrinfo hints{};
  hints.ai_flags = AI_CANONNAME;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* raw_results = nullptr;
  if (getaddrinfo(host.c_str(), nullptr, &hints, &raw_results) == 0) {
    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> results(raw_results,
                                                               freeaddrinfo);
    if (results && results->ai_canonname) {
      std::string canonical = results->ai_canonname;
      if (canonical.find('.') != std::string::npos) {
        return DeriveDiscoveryDomainFromFqdn(canonical);
      }
    }
  }

  Throw("Could not derive a discovery domain from the local host name."
        " Use --discovery-domain or --address.");
}

std::vector<SrvRecord> LookupSrvRecords(const std::string& query_name)
{
  std::array<unsigned char, NS_MAXMSG> answer{};
  const int length
      = res_query(query_name.c_str(), ns_c_in, ns_t_srv, answer.data(),
                  answer.size());
  if (length < 0) {
    Throw("Failed to discover the setup service via DNS SRV for \"" + query_name
          + "\". Use --address or --discovery-domain.");
  }

  ns_msg message{};
  if (ns_initparse(answer.data(), length, &message) < 0) {
    Throw("Failed to parse DNS SRV discovery data for \"" + query_name + "\".");
  }

  std::vector<SrvRecord> records;
  const int answers = ns_msg_count(message, ns_s_an);
  for (int index = 0; index < answers; ++index) {
    ns_rr resource_record{};
    if (ns_parserr(&message, ns_s_an, index, &resource_record) != 0) {
      continue;
    }
    if (ns_rr_type(resource_record) != ns_t_srv) { continue; }

    const unsigned char* rdata = ns_rr_rdata(resource_record);
    if (ns_rr_rdlen(resource_record) < 7) { continue; }

    std::array<char, NS_MAXDNAME> expanded{};
    if (dn_expand(answer.data(), answer.data() + length, rdata + 6,
                  expanded.data(), expanded.size())
        < 0) {
      continue;
    }

    std::string target = expanded.data();
    if (target == ".") {
      Throw("DNS discovery explicitly reports no setup service for \""
            + query_name + "\". Use --address.");
    }

    records.push_back(
        {.priority = static_cast<uint16_t>(ns_get16(rdata)),
         .weight = static_cast<uint16_t>(ns_get16(rdata + 2)),
         .port = static_cast<uint16_t>(ns_get16(rdata + 4)),
         .target = std::move(target)});
  }

  if (records.empty()) {
    Throw("No setup-service SRV records found for \"" + query_name
          + "\". Use --address or --discovery-domain.");
  }
  return records;
}

uint16_t DefaultRandomBelow(uint16_t exclusive_upper_bound)
{
  if (exclusive_upper_bound == 0) { return 0; }
  static thread_local std::mt19937 generator(std::random_device{}());
  std::uniform_int_distribution<uint16_t> distribution(
      0, static_cast<uint16_t>(exclusive_upper_bound - 1));
  return distribution(generator);
}

DiscoveredEndpoint DiscoverSetupService(const std::string& domain,
                                        std::optional<uint16_t> explicit_port,
                                        LookupSrvRecordsFn lookup_fn,
                                        RandomBelowFn random_fn)
{
  if (domain.empty()) {
    Throw("The discovery domain must not be empty.");
  }

  const std::string query_name = "_bareos-setup._tcp." + domain;
  auto records = lookup_fn(query_name);

  const auto lowest_priority = std::min_element(
      records.begin(), records.end(),
      [](const SrvRecord& left, const SrvRecord& right) {
        return left.priority < right.priority;
      })->priority;

  std::vector<SrvRecord> candidates;
  for (const auto& record : records) {
    if (record.priority == lowest_priority) { candidates.push_back(record); }
  }
  if (candidates.empty()) {
    Throw("No usable setup-service SRV records found for \"" + query_name
          + "\".");
  }

  uint32_t total_weight = 0;
  for (const auto& candidate : candidates) { total_weight += candidate.weight; }

  SrvRecord selected = candidates.front();
  if (total_weight > 0) {
    const uint32_t draw = random_fn(static_cast<uint16_t>(total_weight));
    uint32_t cumulative = 0;
    for (const auto& candidate : candidates) {
      cumulative += candidate.weight;
      if (draw < cumulative) {
        selected = candidate;
        break;
      }
    }
  }

  if (selected.target.empty() || selected.target == ".") {
    Throw("The discovered setup-service target is invalid for \"" + query_name
          + "\".");
  }

  return {.query_name = query_name,
          .target = selected.target,
          .port = explicit_port.value_or(selected.port)};
}

}  // namespace setup_service_discovery
