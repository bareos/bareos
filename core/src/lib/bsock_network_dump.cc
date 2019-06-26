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
#include "bsock_network_dump.h"
#include "bsock_network_dump_private.h"
#include "include/make_unique.h"

std::unique_ptr<BnetDump> BnetDump::Create(std::string own_qualified_name)
{
  if (BnetDumpPrivate::filename_.empty()) {
    std::unique_ptr<BnetDump> p;
    return p;
  } else {
    std::unique_ptr<BnetDump> p(std::make_unique<BnetDump>(own_qualified_name));
    return p;
  }
}

BnetDump::BnetDump(const std::string& own_qualified_name) : BnetDump()
{
  impl_->own_qualified_name_ = own_qualified_name;
}

BnetDump::BnetDump() : impl_(std::make_unique<BnetDumpPrivate>())
{
  impl_->OpenFile();
}

BnetDump::~BnetDump()
{
  //
  impl_->CloseFile();
}

bool BnetDump::IsInitialized() const { return impl_->output_file_.is_open(); }

void BnetDump::DisableLogging() { impl_->logging_disabled_ = true; }

void BnetDump::EnableLogging() { impl_->logging_disabled_ = false; }

void BnetDump::SetDestinationQualifiedName(
    std::string destination_qualified_name)
{
  impl_->destination_qualified_name_ = destination_qualified_name;
}

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

  impl_->SaveAndSendMessageIfNoDestinationDefined(ptr, nbytes);
  impl_->DumpToFile(ptr, nbytes);
}
