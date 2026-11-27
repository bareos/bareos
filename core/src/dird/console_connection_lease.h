/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_CONSOLE_CONNECTION_LEASE_H_
#define BAREOS_DIRD_CONSOLE_CONNECTION_LEASE_H_

#include <atomic>
#include "include/baconfig.h"

class ConsoleConnectionLease {
  static std::atomic<std::size_t> num_leases;
  bool has_lease{false};

 public:
  ConsoleConnectionLease()
  {
    has_lease = true;
    num_leases++;
  }
  ConsoleConnectionLease(ConsoleConnectionLease&& lease)
  {
    std::swap(has_lease, lease.has_lease);
  }

  ConsoleConnectionLease& operator=(ConsoleConnectionLease&& lease)
  {
    std::swap(has_lease, lease.has_lease);
    return *this;
  }

  ConsoleConnectionLease(const ConsoleConnectionLease&) = delete;
  ConsoleConnectionLease& operator=(const ConsoleConnectionLease&) = delete;

  ~ConsoleConnectionLease()
  {
    if (!has_lease) { return; }
    auto before = num_leases.fetch_sub(1);
    ASSERT(before > 0);
  }
  static std::size_t get_num_leases() { return num_leases; }
};
#endif  // BAREOS_DIRD_CONSOLE_CONNECTION_LEASE_H_
