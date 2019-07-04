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

#include "include/bareos.h"
#include "bnet_network_dump_private.h"

#include "include/make_unique.h"
#include "lib/ascii_control_characters.h"
#include "lib/backtrace.h"
#include "lib/bareos_resource.h"
#include "lib/bnet.h"
#include "lib/bsock.h"
#include "lib/bsock_tcp.h"
#include "lib/bstringlist.h"
#include "lib/qualified_resource_name_type_converter.h"

#include <cassert>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <set>

std::string BnetDumpPrivate::filename_;
bool BnetDumpPrivate::plantuml_mode_ = false;
std::size_t BnetDumpPrivate::max_data_dump_bytes_ = 100;
int BnetDumpPrivate::stack_level_start_ = 6;
int BnetDumpPrivate::stack_level_amount_ = 0;
std::set<std::string> BnetDumpPrivate::exclude_rcodes_;  //= {"R_CONSOLE"};

void BnetDumpPrivate::OpenFile()
{
  if (!filename_.empty()) {
    output_file_.open(filename_, std::ios::app);
    assert(output_file_.is_open());
  }
}

void BnetDumpPrivate::CloseFile() { output_file_.close(); }

bool BnetDumpPrivate::SetFilename(const char* filename)
{
  BnetDumpPrivate::filename_ = filename;
  return true;
}

std::string BnetDumpPrivate::CreateDataString(int signal,
                                              const char* ptr,
                                              int nbytes) const
{
  std::size_t string_length = nbytes - BareosSocketTCP::header_length;
  string_length = std::min(string_length, max_data_dump_bytes_);

  std::string data_string(&ptr[BareosSocketTCP::header_length], string_length);

  if (signal < 0) {
    data_string =
        BnetSignalToString(signal) + " - " + BnetSignalToDescription(signal);
  }
  std::replace(data_string.begin(), data_string.end(), '\n', ' ');
  std::replace(data_string.begin(), data_string.end(), '\t', ' ');
  data_string.erase(
      std::remove_if(data_string.begin(), data_string.end(),
                     [](char c) { return !isprint(c) || c == '\r'; }),
      data_string.end());

  return data_string;
}

std::string BnetDumpPrivate::CreateFormatStringForNetworkMessage(
    int signal) const
{
  std::string s;
  if (plantuml_mode_) {
    if (signal > 998) {  // signal set to 999
      s = "\"%s\" -> \"%s\": (>%3d) %s\\n";
    } else if (signal < 0) {  // bnet signal
      s = "\"%s\" -> \"%s\": (%4d) %s\\n";
    } else {
      s = "\"%s\" -> \"%s\": (%4d) %s\\n";
    }
  } else {
    if (signal > 998) {  // signal set to 999
      s = "%12s -> %-12s: (>%3d) %s\n";
    } else if (signal < 0) {  // bnet signal
      s = "%12s -> %-12s: (%4d) %s\n";
    } else {
      s = "%12s -> %-12s: (%4d) %s\n";
    }
  }
  return s;
}

bool BnetDumpPrivate::IsExcludedRcode(const BStringList& l) const
{
  if (l.size() > 0) {
    const std::string& probe = l[0];
    if (exclude_rcodes_.find(probe) != exclude_rcodes_.end()) { return true; }
  }
  return false;
}

bool BnetDumpPrivate::SuppressMessageIfRcodeIsInExcludeList() const
{
  BStringList own_name(own_qualified_name_, "::");
  BStringList destination_name(destination_qualified_name_, "::");

  return IsExcludedRcode(own_name) || IsExcludedRcode(destination_name);
}

void BnetDumpPrivate::CreateAndWriteMessageToBuffer(const char* ptr, int nbytes)
{
  static_assert(BareosSocketTCP::header_length == sizeof(int32_t),
                "BareosSocket header size does not match");
  int signal = ntohl(*((int32_t*)&ptr[0]));
  if (signal > 999) { signal = 999; }

  std::vector<char> buffer(1024);

  snprintf(buffer.data(), buffer.size(),
           CreateFormatStringForNetworkMessage(signal).c_str(),
           own_qualified_name_.c_str(), destination_qualified_name_.c_str(),
           signal, CreateDataString(signal, ptr, nbytes).c_str());
  output_buffer_ = buffer.data();
}

void BnetDumpPrivate::CreateAndWriteStacktraceToBuffer()
{
  std::vector<BacktraceInfo> trace_lines(
      Backtrace(stack_level_start_, stack_level_amount_));

  std::vector<char> buffer(1024);
  const char* fmt = plantuml_mode_ ? "(T%3d) %s\\n" : "(T%3d) %s\n";

  for (const BacktraceInfo& bt : trace_lines) {
    std::string s(bt.function_call_.c_str(),
                  std::min(bt.function_call_.size(), max_data_dump_bytes_));
    snprintf(buffer.data(), buffer.size(), fmt, bt.frame_number_, s.c_str());
    output_buffer_ += buffer.data();
  }

  if (plantuml_mode_) { output_buffer_ += "\n"; }
}

void BnetDumpPrivate::DumpToFile(const char* ptr, int nbytes)
{
  if (SuppressMessageIfRcodeIsInExcludeList()) { return; }

  if (state_ == State::kRunNormal) {
    CreateAndWriteMessageToBuffer(ptr, nbytes);
    CreateAndWriteStacktraceToBuffer();
    output_file_ << output_buffer_;
    output_file_.flush();

  } else if (state_ == State::kWaitForDestinationName) {
    return;
  }
}

void BnetDumpPrivate::SaveAndSendMessageIfNoDestinationDefined(const char* ptr,
                                                               int nbytes)
{
  if (state_ != State::kWaitForDestinationName) { return; }

  if (destination_qualified_name_.empty()) {
    std::size_t amount = nbytes;
    amount = std::min(amount, max_data_dump_bytes_);

    std::vector<char> temp_data;
    std::copy(ptr, ptr + amount, std::back_inserter(temp_data));

    temporary_buffer_for_initial_messages_.push_back(temp_data);

    if (temporary_buffer_for_initial_messages_.size() > 3) {
      Dmsg0(100, "BnetDumpPrivate: destination_qualified_name_ not set\n");
    }

  } else {  // !empty() -> send all buffered messages
    state_ = State::kRunNormal;
    for (auto& v : temporary_buffer_for_initial_messages_) {
      DumpToFile(v.data(), v.size());
    }
    temporary_buffer_for_initial_messages_.clear();
  }  // destination_qualified_name_.empty()
}
