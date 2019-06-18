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

#include "bsock_network_dump.h"
#include "include/make_unique.h"
#include "lib/backtrace.h"
#include "lib/bareos_resource.h"
#include "lib/bnet.h"
#include "lib/bstringlist.h"
#include "lib/bsock.h"
#include "lib/bsock_tcp.h"
#include "lib/qualified_resource_name_type_converter.h"

#include <cassert>
#include <algorithm>
#include <execinfo.h>
#include <iostream>
#include <fstream>
#include <set>

class BnetDumpPrivate {
 private:
  friend class BnetDump;

  static std::string filename_;
  static bool plantuml_mode_;
  static std::size_t max_data_dump_bytes_;
  static int stack_level_start_;
  static int stack_level_amount_;
  static std::set<std::string> exclude_rcodes_;

  static bool SetFilename(const char* filename);

  mutable std::string output_buffer_;
  std::ofstream output_file_;

  std::unique_ptr<BareosResource> dummy_resource_;
  const BareosResource* own_resource_ = nullptr;
  const BareosResource* distant_resource_ = nullptr;
  bool logging_disabled_ = false;

  void OpenFile();
  void CloseFile();
  void CreateAndAssignDummyResource(
      int destination_rcode_for_dummy_resource,
      const QualifiedResourceNameTypeConverter& conv);
  std::string CreateDataString(int signal, const char* ptr, int nbytes) const;
  std::string CreateFormatStringForNetworkMessage(int signal) const;
  bool CreateAndWriteMessageToBuffer(const char* ptr, int nbytes) const;
  void CreateAndWriteStacktraceToBuffer() const;
};

std::string BnetDumpPrivate::filename_;
bool BnetDumpPrivate::plantuml_mode_ = true;
std::size_t BnetDumpPrivate::max_data_dump_bytes_ = 64;
int BnetDumpPrivate::stack_level_start_ = 6;
int BnetDumpPrivate::stack_level_amount_ = 0;
std::set<std::string> BnetDumpPrivate::exclude_rcodes_;  // = {"R_CONSOLE"};

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

void BnetDumpPrivate::CreateAndAssignDummyResource(
    int destination_rcode_for_dummy_resource,
    const QualifiedResourceNameTypeConverter& conv)
{
  dummy_resource_ = std::move(std::make_unique<BareosResource>());
  dummy_resource_->rcode_ = destination_rcode_for_dummy_resource;
  dummy_resource_->rcode_str_ =
      conv.ResourceTypeToString(destination_rcode_for_dummy_resource);
  distant_resource_ = dummy_resource_.get();
}

std::string BnetDumpPrivate::CreateDataString(int signal,
                                              const char* ptr,
                                              int nbytes) const
{
  std::size_t string_length = nbytes - BareosSocketTCP::header_length;
  string_length = string_length > max_data_dump_bytes_ ? max_data_dump_bytes_
                                                       : string_length;

  std::string string_data(&ptr[BareosSocketTCP::header_length], string_length);

  if (signal < 0) {
    string_data =
        BnetSignalToString(signal) + " - " + BnetSignalToDescription(signal);
  }
  std::replace(string_data.begin(), string_data.end(), '\n', ' ');
  std::replace(string_data.begin(), string_data.end(), '\t', ' ');
  string_data.erase(
      std::remove_if(string_data.begin(), string_data.end(),
                     [](char c) { return !isprint(c) || c == '\r'; }),
      string_data.end());

  return string_data;
}

std::string BnetDumpPrivate::CreateFormatStringForNetworkMessage(
    int signal) const
{
  std::string s;
  if (plantuml_mode_) {
    if (signal > 998) {  // signal set to 999
      s = "%s -> %s: (>%3d) %s\\n";
    } else if (signal < 0) {  // bnet signal
      s = "%s -> %s: (%4d) %s\\n";
    } else {
      s = "%s -> %s: (%4d) %s\\n";
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

bool BnetDumpPrivate::CreateAndWriteMessageToBuffer(const char* ptr,
                                                    int nbytes) const
{
  int signal = ntohl(*((int32_t*)&ptr[0]));
  if (signal > 999) { signal = 999; }

  const char* own_rcode_str = "UNKNOWN";
  if (own_resource_ && !own_resource_->rcode_str_.empty()) {
    own_rcode_str = own_resource_->rcode_str_.c_str();
  }

  const char* connected_rcode_str = "UNKNOWN";
  if (distant_resource_ && !distant_resource_->rcode_str_.empty()) {
    connected_rcode_str = distant_resource_->rcode_str_.c_str();
  }

  if (exclude_rcodes_.find(own_rcode_str) != exclude_rcodes_.end() ||
      exclude_rcodes_.find(connected_rcode_str) != exclude_rcodes_.end()) {
    return false;
  }

  std::vector<char> buffer(1024);

  snprintf(buffer.data(), buffer.size(),
           CreateFormatStringForNetworkMessage(signal).c_str(), own_rcode_str,
           connected_rcode_str, signal,
           CreateDataString(signal, ptr, nbytes).c_str());
  output_buffer_ = buffer.data();
  return true;
}

void BnetDumpPrivate::CreateAndWriteStacktraceToBuffer() const
{
  std::vector<BacktraceInfo> trace_lines(
      Backtrace(stack_level_start_, stack_level_amount_));

  std::vector<char> buffer(1024);
  const char* fmt = plantuml_mode_ ? "(T%3d) %s\\n" : "(T%3d) %s\n";

  for (const BacktraceInfo& bt : trace_lines) {
    std::string s(bt.function_call_.c_str(),
                  bt.function_call_.size() < max_data_dump_bytes_
                      ? bt.function_call_.size()
                      : max_data_dump_bytes_);
    snprintf(buffer.data(), buffer.size(), fmt, bt.frame_number_, s.c_str());
    output_buffer_ += buffer.data();
  }

  if (plantuml_mode_) { output_buffer_ += "\n"; }
}

std::unique_ptr<BnetDump> BnetDump::Create(
    const BareosResource* own_resource,
    const BareosResource* distant_resource)
{
  std::unique_ptr<BnetDump> p;
  if (BnetDumpPrivate::filename_.empty()) {
    return p;
  } else {
    p = std::move(std::make_unique<BnetDump>(own_resource, distant_resource));
    return p;
  }
}

std::unique_ptr<BnetDump> BnetDump::Create(
    const BareosResource* own_resource,
    int destination_rcode_for_dummy_resource,
    const QualifiedResourceNameTypeConverter& conv)
{
  if (BnetDumpPrivate::filename_.empty()) {
    std::unique_ptr<BnetDump> p;
    return p;
  } else {
    std::unique_ptr<BnetDump> p(std::make_unique<BnetDump>(
        own_resource, destination_rcode_for_dummy_resource, conv));
    return p;
  }
}

BnetDump::BnetDump() : impl_(std::make_unique<BnetDumpPrivate>())
{
  // standard constructor just creates private data object
}

BnetDump::BnetDump(const BareosResource* own_resource,
                   const BareosResource* distant_resource)
    : BnetDump()
{
  impl_->own_resource_ = own_resource;
  impl_->distant_resource_ = distant_resource;
  impl_->OpenFile();
}

BnetDump::BnetDump(const BareosResource* own_resource,
                   int destination_rcode_for_dummy_resource,
                   const QualifiedResourceNameTypeConverter& conv)
    : BnetDump()
{
  impl_->own_resource_ = own_resource;
  impl_->CreateAndAssignDummyResource(destination_rcode_for_dummy_resource,
                                      conv);
  impl_->OpenFile();
}

BnetDump::~BnetDump() { impl_->CloseFile(); }

bool BnetDump::IsInitialized() const { return impl_->output_file_.is_open(); }

void BnetDump::DisableLogging() { impl_->logging_disabled_ = true; }

void BnetDump::EnableLogging() { impl_->logging_disabled_ = false; }

bool BnetDump::EvaluateCommandLineArgs(const char* cmdline_optarg)
{
  if (strlen(optarg) == 1) {
    if (*optarg == 'p') {
      BnetDumpPrivate::plantuml_mode_ = true;
    } else if (std::isdigit(optarg[0]) || optarg[0] == '-') {
      try {
        BnetDumpPrivate::stack_level_amount_ = std::stoi(optarg);
      } catch (const std::exception& e) {
        return false;
      }
    }
  } else if (!BnetDumpPrivate::SetFilename(optarg)) {
    return false;
  }
  return true;
}

void BnetDump::DumpMessageAndStacktraceToFile(const char* ptr, int nbytes) const
{
  if (!IsInitialized() || impl_->logging_disabled_) { return; }

  if (!impl_->CreateAndWriteMessageToBuffer(ptr, nbytes)) { return; }
  impl_->CreateAndWriteStacktraceToBuffer();

  impl_->output_file_ << impl_->output_buffer_;
  impl_->output_file_.flush();
}
