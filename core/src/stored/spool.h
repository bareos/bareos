/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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
#ifndef STORED_SOCKET_H_
#define STORED_SOCKET_H_

bool begin_data_spool (DeviceControlRecord *dcr);
bool discard_data_spool (DeviceControlRecord *dcr);
bool commit_data_spool (DeviceControlRecord *dcr);
bool are_attributes_spooled (JobControlRecord *jcr);
bool begin_attribute_spool (JobControlRecord *jcr);
bool discard_attribute_spool (JobControlRecord *jcr);
bool commit_attribute_spool (JobControlRecord *jcr);
bool write_block_to_spool_file (DeviceControlRecord *dcr);
void list_spool_stats (void sendit(const char *msg, int len, void *sarg), void *arg);

#endif // STORED_SOCKET_H_
