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

std::unique_ptr<BnetDump> BnetDump::Create(
    const BareosResource* own_resource,
    const BareosResource* destination_resource)
{
  std::unique_ptr<BnetDump> p;
  if (BnetDumpPrivate::filename_.empty()) {
    return p;
  } else {
    p = std::move(
        std::make_unique<BnetDump>(own_resource, destination_resource));
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

std::unique_ptr<BnetDump> BnetDump::Create(
    int own_rcode_for_dummy_resource,
    int destination_rcode_for_dummy_resource,
    const QualifiedResourceNameTypeConverter& conv)
{
  if (BnetDumpPrivate::filename_.empty()) {
    std::unique_ptr<BnetDump> p;
    return p;
  } else {
    std::unique_ptr<BnetDump> p(
        std::make_unique<BnetDump>(own_rcode_for_dummy_resource,
                                   destination_rcode_for_dummy_resource, conv));
    return p;
  }
}

BnetDump::BnetDump() : impl_(std::make_unique<BnetDumpPrivate>())
{
  // standard constructor just creates private data object
}

BnetDump::BnetDump(const BareosResource* own_resource,
                   const BareosResource* destination_resource)
    : BnetDump()
{
  impl_->own_resource_ = own_resource;
  impl_->destination_resource_ = destination_resource;
  impl_->OpenFile();
}

BnetDump::BnetDump(const BareosResource* own_resource,
                   int destination_rcode_for_dummy_resource,
                   const QualifiedResourceNameTypeConverter& conv)
    : BnetDump()
{
  impl_->own_resource_ = own_resource;
  impl_->CreateAndAssignDestinationDummyResource(
      destination_rcode_for_dummy_resource, conv);
  impl_->OpenFile();
}

BnetDump::BnetDump(int own_rcode_for_dummy_resource,
                   int destination_rcode_for_dummy_resource,
                   const QualifiedResourceNameTypeConverter& conv)
    : BnetDump()
{
  impl_->CreateAndAssignOwnDummyResource(own_rcode_for_dummy_resource, conv);
  impl_->CreateAndAssignDestinationDummyResource(
      destination_rcode_for_dummy_resource, conv);
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
