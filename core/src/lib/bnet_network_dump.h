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

#ifndef BAREOS_LIB_BNET_NETWORK_DUMP_H_
#define BAREOS_LIB_BNET_NETWORK_DUMP_H_

class BareosResource;
class QualifiedResourceNameTypeConverter;
class BnetDumpPrivate;

class BnetDump {
 public:
  static std::unique_ptr<BnetDump> Create(std::string own_qualified_name);
  static bool EvaluateCommandLineArgs(const char* cmdline_optarg);

  void SetDestinationQualifiedName(std::string destination_qualified_name);
  void DumpMessageAndStacktraceToFile(const char* ptr, int nbytes) const;

 public:
  ~BnetDump();

  BnetDump(const BnetDump& other) = delete;
  BnetDump(const BnetDump&& other) = delete;
  BnetDump& operator=(const BnetDump& rhs) = delete;
  BnetDump& operator=(const BnetDump&& rhs) = delete;
  BnetDump() = delete;

 private:
  BnetDump(const std::string& own_qualified_name);
  std::unique_ptr<BnetDumpPrivate> impl_;
};

#endif  // BAREOS_LIB_BNET_NETWORK_DUMP_H_
