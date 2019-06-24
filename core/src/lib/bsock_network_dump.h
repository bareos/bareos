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

class BareosResource;
class QualifiedResourceNameTypeConverter;
class BnetDumpPrivate;

class BnetDump {
 public:
  static std::unique_ptr<BnetDump> Create(
      const BareosResource* own_resource,
      const BareosResource* distant_resource);
  static std::unique_ptr<BnetDump> Create(
      const BareosResource* own_resource,
      int destination_rcode_for_dummy_resource,
      const QualifiedResourceNameTypeConverter& conv);
  static std::unique_ptr<BnetDump> Create(
      int own_rcode_for_dummy_resource,
      int destination_rcode_for_dummy_resource,
      const QualifiedResourceNameTypeConverter& conv);

  static bool EvaluateCommandLineArgs(const char* cmdline_optarg);

  void DumpMessageAndStacktraceToFile(const char* ptr, int nbytes) const;
  bool IsInitialized() const;
  void DisableLogging();
  void EnableLogging();

 public:
  BnetDump(const BareosResource* own_resource,
           const BareosResource* distant_resource);
  BnetDump(const BareosResource* own_resource,
           int destination_rcode_for_dummy_resource,
           const QualifiedResourceNameTypeConverter& conv);
  BnetDump(int own_rcode_for_dummy_resource,
           int destination_rcode_for_dummy_resource,
           const QualifiedResourceNameTypeConverter& conv);
  BnetDump(const BnetDump& other) = delete;
  BnetDump(const BnetDump&& other) = delete;
  BnetDump& operator=(const BnetDump& rhs) = delete;
  BnetDump& operator=(const BnetDump&& rhs) = delete;
  ~BnetDump();

 private:
  BnetDump();
  std::unique_ptr<BnetDumpPrivate> impl_;
};

#endif  // BAREOS_LIB_BSOCK_NETWORK_DUMP_H_
