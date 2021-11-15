/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2021 Bareos GmbH & Co. KG

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

#ifndef BAREOS_FILED_DIR_CMD_H_
#define BAREOS_FILED_DIR_CMD_H_

class JobControlRecord;
class BareosSocket;

namespace filedaemon {

JobControlRecord* create_new_director_session(BareosSocket* dir);
void* process_director_commands(JobControlRecord* jcr, BareosSocket* dir);
void* handle_director_connection(BareosSocket* dir);
bool StartConnectToDirectorThreads();
bool StopConnectToDirectorThreads(bool wait = false);
} /* namespace filedaemon */

#endif  // BAREOS_FILED_DIR_CMD_H_
