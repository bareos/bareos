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

class BareosSocketNetworkDumpPrivate {
 private:
  friend class BareosSocketNetworkDump;

  static std::string filename_;
  static bool plantuml_mode_;
  static std::size_t max_data_dump_bytes_;

  static bool SetFilename(const char* filename);

  FILE* fp_ = nullptr;
  std::unique_ptr<BareosResource> dummy_resource_;
  const BareosResource* own_resource_ = nullptr;
  const BareosResource* distant_resource_ = nullptr;

  void OpenFile();
  void CloseFile();
  void CreateAndAssignDummyResource(
      int destination_rcode_for_dummy_resource,
      const QualifiedResourceNameTypeConverter& conv);
  std::string CreateDataString(int signal, const char* ptr, int nbytes) const;
  std::string CreateFormatStringForNetworkMessage(int signal) const;
  void DumpStacktrace() const;
};

std::string BareosSocketNetworkDumpPrivate::filename_;
bool BareosSocketNetworkDumpPrivate::plantuml_mode_ = false;
std::size_t BareosSocketNetworkDumpPrivate::max_data_dump_bytes_ = 64;

void BareosSocketNetworkDumpPrivate::OpenFile()
{
  if (!filename_.empty()) {
    fp_ = fopen(filename_.c_str(), "a");
    assert(fp_);
  }
}

void BareosSocketNetworkDumpPrivate::CloseFile()
{
  if (fp_) {
    fclose(fp_);
    fp_ = nullptr;
  }
}

void BareosSocketNetworkDumpPrivate::CreateAndAssignDummyResource(
    int destination_rcode_for_dummy_resource,
    const QualifiedResourceNameTypeConverter& conv)
{
  dummy_resource_ = std::move(std::make_unique<BareosResource>());
  dummy_resource_->rcode_ = destination_rcode_for_dummy_resource;
  dummy_resource_->rcode_str_ =
      conv.ResourceTypeToString(destination_rcode_for_dummy_resource);
  distant_resource_ = dummy_resource_.get();
}

std::string BareosSocketNetworkDumpPrivate::CreateDataString(int signal,
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

void BareosSocketNetworkDumpPrivate::DumpStacktrace() const
{
  BStringList trace_lines(Backtrace(6), '\n');

  for (std::string s : trace_lines) {
    char buffer[1024];
    std::string p(s.c_str(), s.size() < max_data_dump_bytes_
                                 ? s.size()
                                 : max_data_dump_bytes_);
    const char* fmt = plantuml_mode_ ? "%6s %s\\n" : "%6s %s\n";
    int amount = snprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), fmt, "",
                          p.c_str());
    fwrite(buffer, 1, amount, fp_);
    fflush(fp_);
  }
  if (plantuml_mode_) {
    fwrite("\n", 1, 1, fp_);
    fflush(fp_);
  }
}

std::unique_ptr<BareosSocketNetworkDump> BareosSocketNetworkDump::Create(
    const BareosResource* own_resource,
    const BareosResource* distant_resource)
{
  std::unique_ptr<BareosSocketNetworkDump> p;
  if (BareosSocketNetworkDumpPrivate::filename_.empty()) {
    return p;
  } else {
    p = std::move(std::make_unique<BareosSocketNetworkDump>(own_resource,
                                                            distant_resource));
    return p;
  }
}

std::unique_ptr<BareosSocketNetworkDump> BareosSocketNetworkDump::Create(
    const BareosResource* own_resource,
    int destination_rcode_for_dummy_resource,
    const QualifiedResourceNameTypeConverter& conv)
{
  if (BareosSocketNetworkDumpPrivate::filename_.empty()) {
    std::unique_ptr<BareosSocketNetworkDump> p;
    return p;
  } else {
    std::unique_ptr<BareosSocketNetworkDump> p(
        std::make_unique<BareosSocketNetworkDump>(
            own_resource, destination_rcode_for_dummy_resource, conv));
    return p;
  }
}

BareosSocketNetworkDump::BareosSocketNetworkDump()
    : impl_(std::make_unique<BareosSocketNetworkDumpPrivate>())
{
  // standard constructor just creates private data object
}

BareosSocketNetworkDump::BareosSocketNetworkDump(
    const BareosResource* own_resource,
    const BareosResource* distant_resource)
    : BareosSocketNetworkDump()
{
  impl_->own_resource_ = own_resource;
  impl_->distant_resource_ = distant_resource;
  impl_->OpenFile();
}

BareosSocketNetworkDump::BareosSocketNetworkDump(
    const BareosResource* own_resource,
    int destination_rcode_for_dummy_resource,
    const QualifiedResourceNameTypeConverter& conv)
    : BareosSocketNetworkDump()
{
  impl_->own_resource_ = own_resource;
  impl_->CreateAndAssignDummyResource(destination_rcode_for_dummy_resource,
                                      conv);
  impl_->OpenFile();
}

BareosSocketNetworkDump::~BareosSocketNetworkDump() { impl_->CloseFile(); }

bool BareosSocketNetworkDumpPrivate::SetFilename(const char* filename)
{
  BareosSocketNetworkDumpPrivate::filename_ = filename;
  return true;
}

bool BareosSocketNetworkDump::EvaluateCommandLineArgs(
    const char* cmdline_optarg)
{
  if (strlen(optarg) == 1) {
    if (*optarg == 'p') {
      BareosSocketNetworkDumpPrivate::plantuml_mode_ = true;
    }
  } else if (!BareosSocketNetworkDumpPrivate::SetFilename(optarg)) {
    return false;
  }
  return true;
}

std::string BareosSocketNetworkDumpPrivate::CreateFormatStringForNetworkMessage(
    int signal) const
{
  std::string s;
  if (signal > 998) {  // if text too long, signal set to 999
    s = "%12s -> %-12s: (>%3d) %s";
  } else if (signal < 0) {  // bnet signal
    s = "%12s -> %-12s: (%4d) %s";
  } else {
    s = "%12s -> %-12s: (%4d) %s";
  }
  s += plantuml_mode_ ? "\\n" : "\n";
  return s;
}

void BareosSocketNetworkDump::DumpMessageToFile(const char* ptr,
                                                int nbytes) const
{
  if (!impl_->fp_) { return; }

  int signal = ntohl(*((int32_t*)&ptr[0]));
  if (signal > 999) { signal = 999; }

  const char* own_rcode_str =
      impl_->own_resource_ ? impl_->own_resource_->rcode_str_.c_str() : "???";

  const char* connected_rcode_str =
      impl_->distant_resource_ ? impl_->distant_resource_->rcode_str_.c_str()
                               : "???";

  char buffer[1024];
  int amount =
      snprintf(buffer, sizeof(buffer) / sizeof(buffer[0]),
               impl_->CreateFormatStringForNetworkMessage(signal).c_str(),
               own_rcode_str, connected_rcode_str, signal,
               impl_->CreateDataString(signal, ptr, nbytes).c_str());
  fwrite(buffer, 1, amount, impl_->fp_);
  fflush(impl_->fp_);

  impl_->DumpStacktrace();
}
