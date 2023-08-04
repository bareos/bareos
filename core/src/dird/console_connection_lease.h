/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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

class ConsoleConnectionLease {
  static std::atomic<std::size_t> num_leases;

 public:
  ConsoleConnectionLease() { num_leases++; }
  ~ConsoleConnectionLease() { num_leases--; }
  static std::size_t get_num_leases() { return num_leases; }
};
#endif  // BAREOS_DIRD_CONSOLE_CONNECTION_LEASE_H_
