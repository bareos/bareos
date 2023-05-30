/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2023 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_UA_CMDS_H_
#define BAREOS_DIRD_UA_CMDS_H_

#include <map>

namespace directordaemon {

class ClientResource;
class UaContext;

bool Do_a_command(UaContext* ua);
bool DotMessagesCmd(UaContext* ua, const char* cmd);

struct SetDeviceCommand {
  using ArgumentsList = std::map<std::string, std::string>;
  static ArgumentsList ScanCommandLine(UaContext* ua);
  static bool Cmd(UaContext* ua, const char* cmd);
  static bool SendToSd(UaContext* ua,
                       StorageResource* store,
                       const ArgumentsList& arguments);
};

using client_debug_fn = std::function<void(UaContext* ua,
                                           ClientResource* client,
                                           int level,
                                           int trace_flag,
                                           int hangup_flag,
                                           int timestamp_flag,
                                           int perf)>;
void SetDoClientSetdebugFunction(client_debug_fn f);

using storage_debug_fn = std::function<void(UaContext* ua,
                                            StorageResource* store,
                                            int level,
                                            int trace_flag,
					    int timestamp_flag,
					    int perf)>;
void SetDoStorageSetdebugFunction(storage_debug_fn f);

void DoAllSetDebug(UaContext* ua,
                   int level,
                   int trace_flag,
                   int hangup_flag,
                   int timestamp_flag,
                   int perf);
} /* namespace directordaemon */
#endif  // BAREOS_DIRD_UA_CMDS_H_
