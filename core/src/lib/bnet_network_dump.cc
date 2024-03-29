/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2022 Bareos GmbH & Co. KG

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
#include "bnet_network_dump.h"
#include "bnet_network_dump_private.h"

std::unique_ptr<BnetDump> BnetDump::Create(std::string own_qualified_name)
{
  if (BnetDumpPrivate::filename_.empty()) {
    std::unique_ptr<BnetDump> p;
    return p;
  } else {
    // BnetDump constructor should be private
    std::unique_ptr<BnetDump> p(new BnetDump(own_qualified_name));
    return p;
  }
}

BnetDump::BnetDump(const std::string& own_qualified_name)
    : impl_(std::make_unique<BnetDumpPrivate>())
{
  impl_->own_qualified_name_ = own_qualified_name;
  impl_->OpenFile();
}

BnetDump::~BnetDump()
{
  //
  impl_->CloseFile();
}

void BnetDump::SetDestinationQualifiedName(
    std::string destination_qualified_name)
{
  impl_->destination_qualified_name_ = destination_qualified_name;
}

void BnetDump::DumpMessageAndStacktraceToFile(const char* ptr, int nbytes) const
{
  if (!impl_->output_file_.is_open()) { return; }

  impl_->SaveAndSendMessageIfNoDestinationDefined(ptr, nbytes);
  impl_->DumpToFile(ptr, nbytes);
}
