/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_BSOCK_NETWORK_DUMP_H_
#define BAREOS_LIB_BSOCK_NETWORK_DUMP_H_

#include <string>
#include <memory>

class BareosResource;
class QualifiedResourceNameTypeConverter;
class BareosSocketNetworkDumpPrivate;

class BareosSocketNetworkDump {
 public:
  static bool SetFilename(const char* filename);

  static std::unique_ptr<BareosSocketNetworkDump> Create(
      const BareosResource* own_resource,
      const BareosResource* distant_resource);
  static std::unique_ptr<BareosSocketNetworkDump> Create(
      const BareosResource* own_resource,
      int destination_rcode_for_dummy_resource,
      const QualifiedResourceNameTypeConverter& conv);
  void DumpMessageToFile(const char* ptr, int nbytes) const;

  BareosSocketNetworkDump(const BareosResource* own_resource,
                          const BareosResource* distant_resource);
  BareosSocketNetworkDump(const BareosResource* own_resource,
                          int destination_rcode_for_dummy_resource,
                          const QualifiedResourceNameTypeConverter& conv);
  BareosSocketNetworkDump(const BareosSocketNetworkDump& other) = delete;
  BareosSocketNetworkDump(const BareosSocketNetworkDump&& other) = delete;
  BareosSocketNetworkDump& operator=(const BareosSocketNetworkDump& rhs) =
      delete;
  BareosSocketNetworkDump& operator=(const BareosSocketNetworkDump&& rhs) =
      delete;
  ~BareosSocketNetworkDump();

 private:
  BareosSocketNetworkDump();
  std::unique_ptr<BareosSocketNetworkDumpPrivate> impl_;
};

#endif  // BAREOS_LIB_BSOCK_NETWORK_DUMP_H_
