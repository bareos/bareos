/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2022 Bareos GmbH & Co. KG

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
#ifndef BAREOS_STORED_SPOOL_H_
#define BAREOS_STORED_SPOOL_H_

class StatusPacket;

namespace storagedaemon {

bool BeginDataSpool(DeviceControlRecord* dcr);
bool DiscardDataSpool(DeviceControlRecord* dcr);
bool CommitDataSpool(DeviceControlRecord* dcr);
bool AttributesAreSpooled(JobControlRecord* jcr);
bool BeginAttributeSpool(JobControlRecord* jcr);
bool DiscardAttributeSpool(JobControlRecord* jcr);
bool CommitAttributeSpool(JobControlRecord* jcr);
bool WriteBlockToSpoolFile(DeviceControlRecord* dcr);
void ListSpoolStats(StatusPacket* sp);

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_SPOOL_H_
