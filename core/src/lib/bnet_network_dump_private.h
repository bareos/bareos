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

#ifndef BAREOS_LIB_BNET_NETWORK_DUMP_PRIVATE_H_
#define BAREOS_LIB_BNET_NETWORK_DUMP_PRIVATE_H_

#include "lib/bareos_resource.h"

#include <string>
#include <memory>
#include <set>
#include <fstream>

class QualifiedResourceNameTypeConverter;
class BStringList;

class BnetDumpPrivate {
 public:
  BnetDumpPrivate() = default;
  ~BnetDumpPrivate() = default;

  static bool SetFilename(const char* filename);

  void DumpToFile(const char* ptr, int nbytes);
  void SaveAndSendMessageIfNoDestinationDefined(const char* ptr, int nbytes);
  void OpenFile();
  void CloseFile();

  static std::string filename_;
  static bool plantuml_mode_;
  static int stack_level_amount_;

  std::string own_qualified_name_;
  std::string destination_qualified_name_;

  std::ofstream output_file_;
  bool logging_disabled_ = false;

 private:
  void CreateAndWriteStacktraceToBuffer();
  void CreateAndWriteMessageToBuffer(const char* ptr, int nbytes);
  std::string CreateDataString(int signal, const char* ptr, int nbytes) const;
  std::string CreateFormatStringForNetworkMessage(int signal) const;
  bool SuppressMessageIfRcodeIsInExcludeList() const;
  bool IsExcludedRcode(const BStringList& l) const;

  static int stack_level_start_;
  static std::size_t max_data_dump_bytes_;
  static std::set<std::string> exclude_rcodes_;

  std::string output_buffer_;

  std::vector<std::vector<char>> temporary_buffer_for_initial_messages_;

  enum class State
  {
    kWaitForDestinationName,
    kRunNormal
  };
  State state = State::kWaitForDestinationName;
};

#endif  // BAREOS_LIB_BNET_NETWORK_DUMP_PRIVATE_H_
